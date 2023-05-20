// Utilities
use crate::context;
use eyre::{eyre, Result};
use std::path::{Path, PathBuf};

/** Check if an Entry is synced to a file. */
pub fn is_synced(entry: &context::Entry, file: &Path) -> Result<bool> {
    // If it exists, it's synced
    if file.exists() {
        return Ok(true);
    }
    Ok(false)
}

/** Given a base, a key, and a path to download to, tell me what path to download the key to
*/
pub fn relocate_key(base: &str, key: &str, to: &Path) -> Result<PathBuf> {
    // This is pretty trivial
    let rest = key.strip_prefix(base).ok_or(eyre!("Invalid key!"))?;
    let mut buf = to.to_path_buf();
    println!("rest {} buf {:?}", rest, buf);
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
