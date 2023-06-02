use crate::download;
/** Synchronise files */
use crate::{context, context::Context, utils};
use anyhow::{anyhow, Result};
use serde::{Deserialize, Serialize};
use std::collections::HashSet;
use std::convert::TryFrom;
use std::fs::File;
use std::io::Read;
use std::io::Seek;
use std::io::Write;
use std::path::Path;
use tokio::task::JoinSet;
use walkdir::WalkDir;

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
                Ok(b) => {
                    println!("Marking {:?} as synced with {:?}", new_path, b);
                    utils::mark_synced(new_path.as_path(), &b)?;
                    Ok(())
                }
                Err(e) => Err(anyhow!("Cannot fetch {:?}", e)),
            }
        });
        Ok(())
    }

    /** Sync from source key to target directory.
     *
     * @param remove_old_files If true, we'll remove files in the
     * destination directory that were not in the key list.
     */
    pub async fn sync(
        &mut self,
        ctx: &Context,
        source_key: &str,
        target_path: &Path,
        remove_old_files: bool,
    ) -> Result<()> {
        // List the keys
        let entries = ctx.list_objects(source_key).await?;
        println!("There are {} entries in {}", entries.len(), source_key);
        self.sync_keys(ctx, source_key, target_path, &entries, remove_old_files)
            .await
    }

    pub async fn sync_keys(
        &mut self,
        ctx: &Context,
        source_key: &str,
        target_path: &Path,
        entries: &Vec<context::Entry>,
        remove_old_files: bool,
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
                // @TODO: retry on error.
                val??;
            }
        }
        // Remove any files not in the list. Need to do this last, or it will
        // unlink any .part files we were half-way through downloading.
        self.remove_keys_not_in(entries, source_key, target_path)
            .await?;
        Ok(())
    }

    pub async fn remove_keys_not_in(
        &self,
        entries: &Vec<context::Entry>,
        source_key: &str,
        target_path: &Path,
    ) -> Result<()> {
        // Form a set of what should be there
        let mut whitelist: HashSet<String> = HashSet::new();

        // Put everything in the whitelist. Ignore things that don't exist.
        for entry in entries {
            let path_name = utils::relocate_key(source_key, &entry.key, target_path)?;
            if let Ok(canonical) = utils::path_to_canonical_str(&path_name) {
                whitelist.insert(canonical);
            }
        }

        // Walk our directory tree.
        for entry in WalkDir::new(target_path).into_iter().filter_map(|s| s.ok()) {
            if entry.file_type().is_file() {
                let file_name = entry
                    .file_name()
                    .to_str()
                    .ok_or(anyhow!("Cannot convert file name to string"))?;
                if file_name.starts_with(".etag_") || file_name == "meta.json" {
                    // This is an etag marker
                    continue;
                }

                if let Ok(canonical) = utils::path_to_canonical_str(entry.path()) {
                    let should_be_here = whitelist.contains(&canonical);
                    if !should_be_here {
                        println!("Garbage collecting {}", canonical);
                        let _ = std::fs::remove_file(canonical);
                        println!("Fish");
                    }
                }
            }
        }
        println!("Done remove_keys_not_in");
        Ok(())
    }
}
