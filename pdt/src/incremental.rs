/// Retrieve the incremental db.
use crate::context::{Context, INCREMENTAL_NAME};
use crate::download;
use crate::sync;
use eyre::{eyre, Result};
use std::path::Path;

pub struct Incremental<'a> {
    ctx: &'a Context,
}

impl<'a> Incremental<'a> {
    pub fn new(ctx: &'a Context) -> Result<Self> {
        Ok(Incremental { ctx })
    }

    pub fn lockfile_key(&self) -> String {
        format!("{}/{}/.lock", INCREMENTAL_NAME, self.ctx.network_name).to_string()
    }

    pub fn current_tx_block_key(&self) -> String {
        format!(
            "{}/{}/.currentTxBlk",
            INCREMENTAL_NAME, self.ctx.network_name
        )
        .to_string()
    }

    pub async fn is_locked(&self) -> Result<bool> {
        Ok(self
            .ctx
            .maybe_list_object(&self.lockfile_key())
            .await?
            .is_some())
    }

    pub async fn get_max_block(&self) -> Result<i64> {
        let data = self
            .ctx
            .get_key_as_string(&self.current_tx_block_key())
            .await?;
        Ok(data.parse::<i64>()?)
    }

    // Download the persistence increment tarfiles
    pub async fn download_incr_persistence(&self) -> Result<()> {
        let object_root = format!("{}/{}", INCREMENTAL_NAME, self.ctx.network_name);
        println!("Here!");
        let mut target_path = Path::new(&self.ctx.target_path).to_path_buf();
        target_path.push("diff_persistence");
        // List all the persistence diffs
        let entries = self
            .ctx
            .list_objects(&format!("{}/diff_persistence", object_root))
            .await?;
        // Now sync them
        let mut sync = sync::Sync::new(16)?;
        sync.sync_keys(self.ctx, &object_root, &target_path, &entries)
            .await?;
        Ok(())
    }

    // Download the current state of persistence.
    pub async fn download_persistence(&self) -> Result<()> {
        let object_root = format!(
            "{}/{}/{}",
            INCREMENTAL_NAME, self.ctx.network_name, "persistence"
        );
        println!("Here!");
        let mut target_path = Path::new(&self.ctx.target_path).to_path_buf();
        target_path.push("persistence");
        let mut sync = sync::Sync::new(16)?;
        sync.sync(self.ctx, &object_root, target_path.as_path())
            .await
    }
}
