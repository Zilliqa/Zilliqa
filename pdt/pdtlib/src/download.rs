use crate::{context, utils};
use anyhow::{anyhow, Context, Result};
use serde::{Deserialize, Serialize};
use std::convert::TryFrom;
use std::fs;
use std::fs::File;
use std::io::Read;
use std::io::Seek;
use std::io::Write;
/** Download big files by keeping a bitmap of their contents and then emplacing the data into the file
 */
use std::path::{Path, PathBuf};
use tokio_stream::StreamExt;

pub const DEFAULT_DOWNLOAD_BYTES: i64 = 16 << 20;

#[derive(Serialize, Deserialize, Clone, Debug)]
pub struct Status {
    bucket_name: String,
    key: String,
    e_tag: Option<String>,
    nr_bytes: i64,
    block_bytes: i64,
    filled: Vec<bool>,
}

#[derive(Clone)]
pub struct Downloadable {
    file_name: PathBuf,
    meta_name: PathBuf,
    status: Status,
}

#[derive(Clone)]
pub struct Immediate {}

impl Immediate {
    /** Download a file and return the etag we downloaded */
    pub async fn download(
        ctx: &context::Context,
        key: &str,
        dest_file: &Path,
    ) -> Result<Option<String>> {
        // Unlink the destination
        let _ = fs::remove_file(dest_file);

        println!("dest {:?}", dest_file);
        // Create the directory
        let mut parent_dir = PathBuf::from(dest_file);
        parent_dir.pop();
        std::fs::create_dir_all(parent_dir.as_path())
            .context(format!("Creating {:?}", parent_dir))?;
        // Write to a temp file
        let mut tmp_file = PathBuf::from(dest_file);
        let file_name = dest_file
            .file_name()
            .ok_or(anyhow!("Cannot get file name"))?
            .to_str()
            .ok_or(anyhow!("Filename is not UTF-8"))?
            .to_string();
        let tmp_file_name: String = format!("{}.part", file_name);
        tmp_file.pop();
        tmp_file.push(&tmp_file_name.to_string());

        let mut result = ctx.get_object(key).await?;
        let e_tag = result.e_tag().map(|x| x.to_string());
        let mut output_file = File::options()
            .read(true)
            .write(true)
            .create(true)
            .open(tmp_file.as_path())?;
        while let Some(bytes) = result.body.try_next().await? {
            output_file.write(&bytes)?;
        }
        fs::rename(tmp_file.as_path(), dest_file).context(format!(
            "Cannot rename {:?} to {:?}",
            tmp_file.as_path(),
            dest_file
        ))?;
        Ok(e_tag)
    }
}

impl Status {
    pub fn new(
        bucket_name: &str,
        key: &str,
        e_tag: Option<String>,
        nr_bytes: i64,
        block_bytes: i64,
    ) -> Self {
        let nr_blocks = (nr_bytes + block_bytes - 1) / block_bytes;
        let mut flags = Vec::new();
        flags.resize(nr_blocks as usize, false);
        Status {
            bucket_name: bucket_name.to_string(),
            key: key.to_string(),
            e_tag,
            nr_bytes,
            block_bytes,
            filled: flags,
        }
    }

    pub fn read_from(from_path: &Path) -> Result<Option<Self>> {
        if !from_path.exists() {
            return Ok(None);
        }
        let mut file_p = File::open(from_path)?;
        let mut contents = String::new();
        file_p.read_to_string(&mut contents)?;
        Ok(serde_json::from_str(&contents)?)
    }

    pub fn write_to(&self, to_path: &Path) -> Result<()> {
        let mut file = File::create(to_path)?;
        file.write_all(serde_json::to_string(self)?.as_bytes())?;
        Ok(())
    }
}

impl Downloadable {
    pub fn new(
        bucket_name: &str,
        key: &str,
        file_name: &Path,
        meta_name: &Path,
        size: i64,
        e_tag: Option<String>,
        block_size: i64,
    ) -> Result<Self> {
        // try to load status.
        let from_disk = Status::read_from(meta_name)?;
        let mut status = Status::new(bucket_name, key, e_tag, size, block_size);
        let mut reuse_status = true;

        // Make sure the output file exists and is the right size.
        {
            let data_file = File::options()
                .read(true)
                .write(true)
                .create(true)
                .open(file_name)?;
            let file_size = data_file.metadata()?.len();
            if i64::try_from(file_size)? != size {
                println!("Size mismatch: {} vs {} - redownloading", file_size, size);
                reuse_status = false;
            }
            data_file.set_len(u64::try_from(status.nr_bytes)?)?;
        }

        if reuse_status {
            if let Some(from_disk_status) = from_disk {
                if can_reuse(&from_disk_status, &status) {
                    println!("Resuming download");
                    status = from_disk_status
                }
            }
        }

        Ok(Downloadable {
            file_name: file_name.to_path_buf(),
            meta_name: meta_name.to_path_buf(),
            status,
        })
    }

    pub fn get_outstanding_segments(&self) -> Vec<i64> {
        let mut result = Vec::new();

        for idx in 0..self.status.filled.len() {
            if !self.status.filled[idx] {
                result.push(idx as i64);
            }
        }

        result
    }

    /// Fetch a segment
    /// @return true if the fetch succeeded, false if the file has changed and we need to redownload.
    pub async fn fetch_segment(&self, ctx: &context::Context, seg: i64) -> Result<bool> {
        let idx = seg as usize;
        // If we've already filled it, we're good.
        if self.status.filled[idx] {
            println!("Segment {} already fetched", idx);
            return Ok(true);
        }

        // OK. Download it ..
        println!("Retrieving block {} of {}", idx, self.status.key);
        let start_byte = seg * self.status.block_bytes;

        let mut result = ctx
            .get_byte_range(&self.status.key, start_byte, self.status.block_bytes)
            .await?;
        println!(
            "Retrieved {} etag {:?}",
            result.content_length(),
            result.e_tag()
        );
        if !utils::etag_eq(
            self.status.e_tag.clone(),
            result.e_tag().map(|x| x.to_string()),
        ) {
            // etag changed
            return Ok(false);
        }

        // Emplace it
        println!(
            "Emplacing {} bytes at {}",
            result.content_length(),
            start_byte
        );
        {
            let mut data_file = File::options()
                .read(true)
                .write(true)
                .open(&self.file_name)?;
            data_file.seek(std::io::SeekFrom::Start(u64::try_from(start_byte)?))?;
            while let Some(bytes) = result.body.try_next().await? {
                data_file.write(&bytes)?;
            }
        }

        Ok(true)
    }

    pub fn signal_filled(&mut self, seg: i64) -> Result<()> {
        let idx = seg as usize;
        // Write status
        self.status.filled[idx] = true;
        Ok(())
    }

    pub fn write_status(&mut self) -> Result<()> {
        self.status.write_to(&self.meta_name)?;
        Ok(())
    }

    /// Download is complete when all the segments are downloaded.
    pub fn is_complete(&self) -> bool {
        self.status.filled.iter().fold(true, |v, item| v && !!item)
    }
}

fn can_reuse(reuse_this: &Status, as_this: &Status) -> bool {
    if reuse_this.bucket_name == as_this.bucket_name
        && reuse_this.key == as_this.key
        && reuse_this.e_tag == as_this.e_tag
        && reuse_this.nr_bytes == as_this.nr_bytes
        && reuse_this.block_bytes == as_this.block_bytes
    {
        true
    } else {
        false
    }
}
