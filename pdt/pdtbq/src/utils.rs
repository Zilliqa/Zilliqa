// Here
#[allow(unused_imports)]
use anyhow::{anyhow, Result};
use pdtlib::proto::ByteArray;
use primitive_types::H160;
use sha2::{Digest, Sha256};

/// address_from_public_key() but generate hex.
pub fn maybe_hex_address_from_public_key(pubkey: &[u8]) -> Option<String> {
    match address_from_public_key(pubkey) {
        Err(_) => None,
        Ok(val) => Some(hex::encode(val.as_bytes())),
    }
}

/// Address from public key.
/// Given a pubkey (without the leading 0x), produce an address (without leading 0x)
/// Following the code in zilliqa-js/crypto/util.ts:getAddressFromPublicKey()
pub fn address_from_public_key(pubkey: &[u8]) -> Result<H160> {
    let mut hasher = Sha256::new();
    hasher.update(pubkey);
    let result = hasher.finalize();
    // Lop off the first 12 bytes.
    Ok(H160::from_slice(&result[12..]))
}

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

#[test]
fn check_address_from_pubkey() {
    let data_points: Vec<(&str, &str)> = vec![
        (
            "0246E7178DC8253201101E18FD6F6EB9972451D121FC57AA2A06DD5C111E58DC6A",
            "9BFEC715a6bD658fCb62B0f8cc9BFa2ADE71434A",
        ),
        (
            "02c261017f4299f0e60d33035d7fbc4b85cbaa11cd7127e0a420b331cd805e517a",
            "acd9339df14af808af1f46a3edb7466590199ee6",
        ),
    ];
    for (pubkey, addr) in data_points {
        let dec_addr = address_from_public_key(&hex::decode(pubkey).unwrap()).unwrap();
        let hex_addr = hex::encode(dec_addr);
        assert_eq!(hex_addr.to_lowercase(), addr.to_lowercase());
    }
}
