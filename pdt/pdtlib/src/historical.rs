/** Download historical data
 */
use crate::context::{Context, PERSISTENCE_SNAPSHOT_NAME};
use crate::download;
use anyhow::{anyhow, Result};
use std::fs;
use std::path::Path;

pub struct Historical<'a> {
    ctx: &'a Context,
}

impl<'a> Historical<'a> {
    pub fn new(ctx: &'a Context) -> Result<Self> {
        Ok(Historical { ctx })
    }

    /// Download historical persistence and unpack it into the target directory.
    /// We do this by downloading parts of the file into <tgt>/buffer/<filename>.<start_byte>
    /// Once we have all the bits, we assemble them and unpack. if that fails, we delete
    /// the whole directory and try again. this allows downloads to multithread and to
    /// resume, at the cost that a resume might not work if the wrong bytes were downloaded.
    pub async fn download(&self) -> Result<()> {
        let file_name = format!("{}.tar.gz", self.ctx.network_name);
        let object_key = format!(
            "{}/{}/{}",
            PERSISTENCE_SNAPSHOT_NAME, self.ctx.network_name, file_name
        );
        // Download it, resuming if necessary.
        let mut dest_dir = Path::new(&self.ctx.target_path).to_path_buf();
        dest_dir.push("historical-data");
        // Let's make sure the directory exists.
        {
            let dest_path = dest_dir.as_path();
            if !dest_path.exists() {
                fs::create_dir_all(dest_path)?;
            }
        }
        println!("object key {:?}", object_key);
        let entry = self.ctx.list_object(&object_key).await?;
        let mut main_file = dest_dir.clone();
        let mut meta_file = dest_dir.clone();
        main_file.push(file_name);
        meta_file.push(format!("{}.meta.json", self.ctx.network_name));
        let mut down = download::Downloadable::new(
            &self.ctx.bucket_name.to_string(),
            &object_key,
            main_file.as_path(),
            meta_file.as_path(),
            entry.size,
            entry.e_tag,
            download::DEFAULT_DOWNLOAD_BYTES,
        )?;
        let segs = down.get_outstanding_segments();
        println!("There are {} segments to download .. ", segs.len());
        let mut seg_ptr = 0;
        while seg_ptr < segs.len() {
            let mut handles = Vec::new();
            let mut segments: Vec<i64> = Vec::new();
            for _task in 0..4 {
                if seg_ptr < segs.len() {
                    let my_clone = Box::new(down.clone());
                    let to_retrieve = segs[seg_ptr];
                    let ctx = Context::duplicate(&self.ctx).await?;
                    println!("Fetching {}", segs[seg_ptr]);
                    handles.push(tokio::spawn(async move {
                        match my_clone.fetch_segment(&ctx, to_retrieve).await {
                            Ok(b) => Ok(b),
                            Err(_) => Err(anyhow!("Cannot fetch")),
                        }
                    }));
                    segments.push(segs[seg_ptr]);
                    seg_ptr += 1;
                }
            }
            let results = futures::future::join_all(handles).await;
            for res in results {
                match res? {
                    Err(x) => return Err(x),
                    Ok(false) => return Err(anyhow!("File has changed")),
                    _ => (),
                }
            }

            segments.iter().try_for_each(|x| down.signal_filled(*x))?;
            down.write_status()?;
        }
        println!("All done");

        Ok(())
    }
}
