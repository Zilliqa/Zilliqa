/** Facilities for rendering downloaded persistence deltas.
 *
 */
use crate::utils;
use anyhow::{anyhow, Result};
use std::collections::{HashMap, HashSet};
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

pub enum IncrementalKind {
    Persistence,
    StateDelta,
}

pub struct RecoveryPoints {
    pub persistence_blocks: Vec<i64>,
    pub state_delta_blocks: Vec<i64>,
    pub recovery_points: Vec<i64>,
}

impl Renderer {
    pub fn new(network_name: &str, download_dir: &str, unpack_dir: &str) -> Result<Self> {
        Ok(Renderer {
            network_name: network_name.to_string(),
            download_dir: download_dir.to_string(),
            unpack_dir: unpack_dir.to_string(),
        })
    }

    fn get_file_map(&self, kind: IncrementalKind) -> Result<HashMap<i64, PathBuf>> {
        let diff_dir = Path::new(&self.download_dir).join(match kind {
            IncrementalKind::Persistence => utils::DIR_PERSISTENCE_DIFFS,
            IncrementalKind::StateDelta => utils::DIR_STATEDELTA,
        });
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
                        if let Some(blk_trail) = file_name.strip_prefix(match kind {
                            IncrementalKind::Persistence => utils::PERSISTENCE_DIFF_FILE_PREFIX,
                            IncrementalKind::StateDelta => utils::STATE_DELTA_DIFF_FILE_PREFIX,
                        }) {
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
            .ok_or(anyhow!("Cannot render path for historical data"))?;
        let out = Command::new("tar")
            .args(["-C", &self.unpack_dir, "-xvzf", &source_str])
            .output()?;
        println!("{}", std::str::from_utf8(&out.stdout)?);
        println!("{}", std::str::from_utf8(&out.stderr)?);
        Ok(())
    }

    // Unpack the state delta for block `block`
    pub fn unpack_statedelta(&self, block: i64) -> Result<()> {
        let map = self.get_file_map(IncrementalKind::StateDelta)?;
        let path = map
            .get(&block)
            .ok_or(anyhow!("Cannot find path for state delta {}", block))?;
        let path_str = utils::path_to_str(path)?;
        let tgt_dir = Path::new(&self.unpack_dir).join("stateDelta");
        println!("Unpacking {}", &path_str);
        let out = Command::new("tar")
            .args([
                "-C",
                &utils::path_to_str(&tgt_dir)?,
                "--strip-components",
                "1",
                "-xvzf",
                &path_str,
            ])
            .output()?;
        println!("{:}", std::str::from_utf8(&out.stdout)?);
        Ok(())
    }

    // Unpack blocks into the target.
    pub fn unpack_persistence_deltas(&self, blocks: &Vec<i64>) -> Result<()> {
        let map = self.get_file_map(IncrementalKind::Persistence)?;
        for blk in blocks {
            // What's the filename?
            let path = map.get(blk).ok_or(anyhow!(
                "Cannot find path for incremental persistence {}",
                blk
            ))?;
            let path_str = path
                .to_str()
                .ok_or(anyhow!("Cannot render path for block {}", blk))?;
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
                    .ok_or(anyhow!("Cannot render source path"))?,
                &self.unpack_dir,
            ])
            .output()?;
        Ok(())
    }

    /// List incremental .tar.gz files in block order.
    pub fn list_incremental_blocks(&self, which: IncrementalKind) -> Result<Vec<i64>> {
        let map = self.get_file_map(which)?;
        let mut result: Vec<i64> = map.into_keys().collect();
        result.sort();
        Ok(result)
    }

    pub fn get_recovery_points(&self) -> Result<RecoveryPoints> {
        let persistence_blocks = self.list_incremental_blocks(IncrementalKind::Persistence)?;
        let state_delta_blocks = self.list_incremental_blocks(IncrementalKind::StateDelta)?;
        let state_delta_set: HashSet<i64> = HashSet::from_iter(state_delta_blocks.iter().cloned());

        // All persistence blocks also in the state delta set are recovery points.
        let recovery_points = persistence_blocks
            .iter()
            .filter(|x| state_delta_set.contains(x))
            .cloned()
            .collect::<Vec<i64>>();

        Ok(RecoveryPoints {
            persistence_blocks,
            state_delta_blocks,
            recovery_points,
        })
    }

    /** Unpack.
     *
     * @param recover_from a block to recover from, or None for the latest.
     */
    pub fn unpack(
        &self,
        recovery_points: &RecoveryPoints,
        recover_from: Option<i64>,
    ) -> Result<()> {
        // Build a set of recovery blocks.
        let recover_blk = match recover_from {
            Some(x) => x,
            None => {
                *(recovery_points
                    .recovery_points
                    .last()
                    .ok_or(anyhow!("no valid recovery points"))?)
            }
        };

        let delta_blocks = recovery_points
            .persistence_blocks
            .iter()
            .filter_map(|x| if *x <= recover_blk { Some(*x) } else { None })
            .collect::<Vec<i64>>();

        self.clean_output()?;
        self.unpack_history()?;
        self.copy_current()?;
        self.unpack_persistence_deltas(&delta_blocks)?;
        self.unpack_statedelta(recover_blk)?;
        Ok(())
    }
}
