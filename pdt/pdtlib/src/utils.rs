// Utilities
use crate::context;
use anyhow::{anyhow, Result};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

/* Where do we store persistence diffs? */
pub const DIR_PERSISTENCE_DIFFS: &str = "diff_persistence";
pub const DIR_PERSISTENCE: &str = "persistence";
pub const DIR_HISTORICAL_DATA: &str = "historical-data";
pub const PERSISTENCE_DIFF_FILE_PREFIX: &str = "diff_persistence_";
pub const STATE_DELTA_DIFF_FILE_PREFIX: &str = "stateDelta_";
pub const DIR_STATEDELTA: &str = "statedelta";

// Duplicate a directory. The contents of from will appear in to
// (which will be created if it doesn't exist)
pub fn dup_directory(from: &str, to: &str) -> Result<()> {
    let src_dir = Path::new(from).join(".");
    let dest_dir = Path::new(to);
    let dest_dir_with_dot = Path::new(to).join(".");
    let _ = std::fs::create_dir_all(&dest_dir);
    Command::new("rsync")
        .args([
            "-az",
            src_dir
                .to_str()
                .ok_or(anyhow!("Cannot render source path"))?,
            dest_dir_with_dot
                .to_str()
                .ok_or(anyhow!("Cannot render dest path"))?,
        ])
        .output()?;
    Ok(())
}

pub fn get_etag_name(file: &Path) -> Result<PathBuf> {
    let mut sync_file = PathBuf::from(file);
    let file_name = sync_file.file_name().ok_or(anyhow!(format!(
        "No file name when trying to mark synced {:?}",
        file
    )))?;
    let str_file_name = file_name
        .to_str()
        .ok_or(anyhow!("Cannot convert path name to string"))?
        .to_string();
    let tag_file_name = format!(".etag_{}", str_file_name);
    sync_file.set_file_name(&tag_file_name);
    Ok(sync_file)
}

pub fn mark_synced(file: &Path, e_tag: &Option<String>) -> Result<()> {
    let etag_name = get_etag_name(file)?;
    if let Some(tag) = e_tag {
        fs::write(etag_name, tag)?;
    } else {
        fs::remove_file(etag_name)?;
    }
    Ok(())
}

/** Check if an Entry is synced to a file. */
/** This is hard, because we need to make sure we have the right version. */
pub fn is_synced(entry: &context::Entry, file: &Path) -> Result<bool> {
    // If it has no e_tag, there's no way to tell
    // If we don't have it at all, it can't be synced.
    if !file.exists() {
        return Ok(false);
    }
    match &entry.e_tag {
        Some(tag) => match fs::read_to_string(get_etag_name(file)?) {
            Ok(file_tag) => {
                println!("Tag {} stored {} eq? {} ", tag, file_tag, &file_tag == tag);
                Ok(&file_tag == tag)
            }
            Err(_) => {
                println!("Tag {} none stored", tag);
                Ok(false)
            }
        },
        None => Ok(false),
    }
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
