/// Retrieve the incremental db.
use crate::context::{Context, INCREMENTAL_NAME};
use eyre::{eyre, Result};

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

    // Download the current state of persistence.
    pub async fn download_persistence(&self) -> Result<()> {
        let object_root = foramt!("{}/{}/persistence@, 
    }
}
