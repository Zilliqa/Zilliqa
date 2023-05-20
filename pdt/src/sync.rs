use crate::download;
/** Synchronise files */
use crate::{context, context::Context, utils};
use eyre::{eyre, Result};
use serde::{Deserialize, Serialize};
use std::convert::TryFrom;
use std::fs::File;
use std::io::Read;
use std::io::Seek;
use std::io::Write;
use std::path::Path;
use tokio::task::JoinSet;

#[derive(Debug)]
pub struct Sync {
    /// Number of jobs
    job_count: usize,
    /// Jobset
    jobs: JoinSet<Result<()>>,
}

impl Sync {
    pub fn new(job_count: usize) -> Result<Self> {
        Ok(Sync {
            job_count,
            jobs: JoinSet::new(),
        })
    }

    pub async fn download(
        &mut self,
        ctx: &Context,
        source_key: &str,
        target_path: &Path,
    ) -> Result<()> {
        // Wait for a task handle
        println!("Jobs len {}", self.jobs.len());
        while self.jobs.len() >= self.job_count {
            // Wait for a job to exit
            let result = self.jobs.join_next().await;
            if let Some(res) = result {
                let inner = res?;
                println!("Got result {:?}", inner);
                let _ = inner?;
            }
        }

        let new_key = source_key.to_string();
        let new_path = target_path.to_path_buf();
        let new_context = Context::duplicate(&ctx).await?;

        println!("Spawn ..");
        // Spawn a job to download the file.
        self.jobs.spawn(async move {
            match download::Immediate::download(&new_context, &new_key, &new_path).await {
                Ok(b) => Ok(b),
                Err(e) => Err(eyre!("Cannot fetch {:?}", e)),
            }
        });
        Ok(())
    }

    pub async fn sync(
        &mut self,
        ctx: &Context,
        source_key: &str,
        target_path: &Path,
    ) -> Result<()> {
        // List the keys
        let entries = ctx.list_objects(source_key).await?;
        println!("There are {} entries in {}", entries.len(), source_key);
        self.sync_keys(ctx, source_key, target_path, &entries).await
    }

    pub async fn sync_keys(
        &mut self,
        ctx: &Context,
        source_key: &str,
        target_path: &Path,
        entries: &Vec<context::Entry>,
    ) -> Result<()> {
        // Now sync each key in turn.
        for entry in entries {
            println!("Downloading {}", entry.key);
            let path = utils::relocate_key(source_key, &entry.key, target_path)?;
            if !utils::is_synced(&entry, &path.as_path())? {
                // Not synced. download it!
                self.download(ctx, &entry.key, &path.as_path()).await?;
            }
        }
        // We're at the end; wait for all jobs to be finished.
        while !self.jobs.is_empty() {
            println!("Waiting for {} jobs remaining", self.jobs.len());
            let ended = self.jobs.join_next().await;
            if ended.is_none() {
                break;
            }
            if let Some(val) = ended {
                val?;
            }
        }
        Ok(())
    }
}
