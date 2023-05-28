// Utilities
use crate::context;
use anyhow::{anyhow, Result};
use std::path::{Path, PathBuf};

/* Where do we store persistence diffs? */
pub const DIR_PERSISTENCE_DIFFS: &str = "diff_persistence";
pub const DIR_PERSISTENCE: &str = "persistence";
pub const DIR_HISTORICAL_DATA: &str = "historical-data";
pub const PERSISTENCE_DIFF_FILE_PREFIX: &str = "diff_persistence_";
pub const STATE_DELTA_DIFF_FILE_PREFIX: &str = "stateDelta_";
pub const DIR_STATEDELTA: &str = "statedelta";

/** Check if an Entry is synced to a file. */
pub fn is_synced(entry: &context::Entry, file: &Path) -> Result<bool> {
    // If it exists, it's synced
    if file.exists() {
        return Ok(true);
    }
    Ok(false)
}

/** Convert a path to str or fail */
pub fn path_to_str(path: &Path) -> Result<String> {
    Ok(path
        .to_str()
        .ok_or(anyhow!("Could not convert path"))?
        .to_string())
}

pub fn path_to_canonical_str(path: &Path) -> Result<String> {
    Ok(path_to_str(&std::fs::canonicalize(path)?)?)
}

/** Given a base, a key, and a path to download to, tell me what path to download the key to
*/
pub fn relocate_key(base: &str, key: &str, to: &Path) -> Result<PathBuf> {
    // This is pretty trivial
    let rest = key.strip_prefix(base).ok_or(anyhow!("Invalid key!"))?;
    let mut buf = to.to_path_buf();
    buf.push(Path::new(&format!("./{}", &rest)));
    Ok(buf)
}

pub fn etag_eq(a: Option<String>, b: Option<String>) -> bool {
    if a.is_none() {
        return b.is_none();
    } else if let Some(x) = a {
        if let Some(y) = b {
            return x == y;
        }
    }
    false
}
