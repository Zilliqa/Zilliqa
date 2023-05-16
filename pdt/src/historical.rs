/** Download historical data
 */
use crate::context::{Context, PERSISTENCE_SNAPSHOT_NAME};
use eyre::Result;
use std::fs;
use std::path::{Path, PathBuf};

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
        dest_dir.push("buffer");
        // Let's make sure the directory exists.
        {
            let dest_path = dest_dir.as_path();
            if !dest_path.exists() {
                fs::create_dir_all(dest_path)?;
            }
        }
        println!("object key {:?}", object_key);
        let entry = self.ctx.list_object(&object_key).await?;
        // Now we have a range we want.

        // Find the size of the object and its etag
        //let attrs = self.ctx.get_attributes(&object_key).await?;
        //println!(
        //"Length {:?} , etag {:?}",
        //attrs.e_tag(),
        //attrs.object_size()
        //);
        // What segments already exist?
        Ok(())
    }
}
