// Here
#[allow(unused_imports)]
use anyhow::{anyhow, Result};
use pdtlib::proto::ByteArray;

pub fn u128_string_from_storage(val: &ByteArray) -> Option<String> {
    u128_from_storage(val)
        .ok()
        .and_then(|x| Some(x.to_string()))
}

pub fn u128_from_storage(val: &ByteArray) -> Result<u128> {
    let the_bytes: [u8; 16] = val.data[0..].try_into()?;
    Ok(u128::from_be_bytes(the_bytes))
}

pub fn str_from_u8(val: Option<ByteArray>) -> Result<Option<String>> {
    if let Some(inner) = val {
        Ok(Some(std::str::from_utf8(&inner.data)?.to_string()))
    } else {
        Ok(None)
    }
}
