// Utilities

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
