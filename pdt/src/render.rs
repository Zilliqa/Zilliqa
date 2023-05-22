/** Facilities for rendering downloaded persistence deltas.
 *
 */
use crate::utils;
use eyre::{eyre, Result};
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::{fs, path};

/** Tracks state for rendering from a download directory to an instantiation directory
 *  Essentially, we:
 *
 *   - Copy the current persistence directory, using `cp -l` (which is a cop-out, but a fast one)
 *   - Find all the diffs, in ascending block number order, and apply them.
 *
 *  The latter shouldn't be necessary, but it is because of the way uploads are not locked, so
 *  the only way to "un-corrupt" the files that are currently being corrupted by being uploaded to
 *  persistence, is to copy the thing you wanted over them (kinda)
 */
pub struct Renderer {
    network_name: String,
    download_dir: String,
    unpack_dir: String,
}

impl Renderer {
    pub fn new(network_name: &str, download_dir: &str, unpack_dir: &str) -> Result<Self> {
        Ok(Renderer {
            network_name: network_name.to_string(),
            download_dir: download_dir.to_string(),
            unpack_dir: unpack_dir.to_string(),
        })
    }

    fn get_file_map(&self) -> Result<HashMap<i64, PathBuf>> {
        let diff_dir = Path::new(&self.download_dir).join(utils::DIR_PERSISTENCE_DIFFS);
        let paths = fs::read_dir(diff_dir)?;
        let mut result: HashMap<i64, PathBuf> = HashMap::new();

        for path in paths {
            if let Ok(here) = path {
                let this_path = here.path();
                let maybe_file_name = this_path.file_name();
                if let Some(file_name_as_osstr) = maybe_file_name {
                    let file_name_maybe = file_name_as_osstr.to_os_string().into_string();
                    if let Ok(file_name) = file_name_maybe {
                        // Phew!
                        if let Some(blk_trail) =
                            file_name.strip_prefix(utils::PERSISTENCE_DIFF_FILE_PREFIX)
                        {
                            // OK. We're a valid file.
                            let splits: Vec<_> = blk_trail.split('.').collect();
                            if splits.len() > 0 {
                                result.insert(splits[0].parse::<i64>()?, this_path.to_owned());
                            }
                        }
                    }
                }
            }
        }
        Ok(result)
    }

    // Unpack historical data into the target
    pub fn unpack_history(&self) -> Result<()> {
        let source_file = Path::new(&self.download_dir)
            .join(utils::DIR_HISTORICAL_DATA)
            .join(format!("{}.tar.gz", self.network_name));
        let source_str = source_file
            .to_str()
            .ok_or(eyre!("Cannot render path for historical data"))?;
        let out = Command::new("tar")
            .args(["-C", &self.unpack_dir, "-xvzf", &source_str])
            .output()?;
        println!("{}", std::str::from_utf8(&out.stdout)?);
        println!("{}", std::str::from_utf8(&out.stderr)?);
        Ok(())
    }

    // Unpack blocks into the target.
    pub fn unpack_blocks(&self, blocks: &Vec<i64>) -> Result<()> {
        let map = self.get_file_map()?;
        for blk in blocks {
            // What's the filename?
            let path = map.get(blk).ok_or(eyre!(
                "Cannot find path for incremental persistence {}",
                blk
            ))?;
            let path_str = path
                .to_str()
                .ok_or(eyre!("Cannot render path for block {}", blk))?;
            // OK. Now unpack it.
            println!("Unpacking {}", &path_str);
            let out = Command::new("tar")
                .args([
                    "-C",
                    &self.unpack_dir,
                    "--strip-components",
                    "1",
                    "-xvzf",
                    &path_str,
                ])
                .output()?;
            println!("{:}", std::str::from_utf8(&out.stdout)?);
        }
        Ok(())
    }

    pub fn clean_output(&self) -> Result<()> {
        // Clean up the target directory. Don't care if we fail.
        let _ = std::fs::remove_dir_all(&self.unpack_dir);

        // Make sure the target directory exists.
        std::fs::create_dir_all(Path::new(&self.unpack_dir))?;

        Ok(())
    }

    // Copy the current persistence, by hard linking.
    pub fn copy_current(&self) -> Result<()> {
        let source_dir = Path::new(&self.download_dir)
            .join(utils::DIR_PERSISTENCE)
            .join(".");

        // Now Copy persistence into it.
        Command::new("cp")
            .args([
                "-ir",
                source_dir
                    .to_str()
                    .ok_or(eyre!("Cannot render source path"))?,
                &self.unpack_dir,
            ])
            .output()?;
        Ok(())
    }

    /// List incremental .tar.gz files in block order.
    pub fn list_incrementals(&self) -> Result<Vec<i64>> {
        let map = self.get_file_map()?;
        let mut result: Vec<i64> = map.into_keys().collect();
        result.sort();
        Ok(result)
    }

    /// Unpack
    pub fn unpack(&self, blocks: &Vec<i64>) -> Result<()> {
        self.clean_output()?;
        self.unpack_history()?;
        self.copy_current()?;
        self.unpack_blocks(blocks)?;
        Ok(())
    }
}
