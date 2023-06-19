// Here
#[allow(unused_imports)]
use anyhow::{anyhow, Result};
use pdtlib::proto::ByteArray;
use primitive_types::H160;
use sha2::{Digest, Sha256};
use sha3::Keccak256;

#[derive(Clone, Debug)]
pub struct ProcessCoordinates {
    /// How many machines are processing this dataset?
    pub nr_machines: i64,
    /// How many blocks are there to process?
    pub nr_blks: i64,
    /// How many blocks in a batch?
    pub batch_blks: i64,
    /// What is the id of the machine currently running?
    pub machine_id: i64,
    /// A name for this machine, to print in logs.
    pub client_id: String,
}

#[derive(Clone, Debug)]
pub struct BigQueryDatasetLocation {
    pub project_id: String,
    pub dataset_id: String,
}

impl BigQueryDatasetLocation {
    pub fn get_dataset_desc(&self) -> String {
        format!("{}.{}", self.project_id, self.dataset_id)
    }
}

#[derive(Clone, Debug)]
pub struct BigQueryTableLocation {
    pub dataset: BigQueryDatasetLocation,
    pub table_id: String,
}

impl BigQueryTableLocation {
    pub fn new(bq: &BigQueryDatasetLocation, table_id: &str) -> Self {
        BigQueryTableLocation {
            dataset: bq.clone(),
            table_id: table_id.to_string(),
        }
    }

    pub fn to_meta(&self) -> BigQueryTableLocation {
        BigQueryTableLocation {
            dataset: self.dataset.clone(),
            table_id: format!("{}_meta", self.table_id),
        }
    }

    pub fn get_table_desc(&self) -> String {
        format!("{}.{}", self.dataset.get_dataset_desc(), self.table_id)
    }
}

pub enum API {
    Ethereum,
    Zilliqa,
}

/// address_from_public_key() but generate hex.
pub fn maybe_hex_address_from_public_key(pubkey: &[u8], api: API) -> Option<String> {
    match address_from_public_key(pubkey, api) {
        Err(_) => None,
        Ok(val) => Some(hex::encode(val.as_bytes())),
    }
}

/// Address from public key.
/// Given a pubkey (without the leading 0x), produce an address (without leading 0x)
/// Following the code in zilliqa-js/crypto/util.ts:getAddressFromPublicKey()
pub fn address_from_public_key(pubkey: &[u8], api: API) -> Result<H160> {
    match api {
        API::Ethereum => {
            let mut hasher = Keccak256::new();
            hasher.update(pubkey);
            let result = hasher.finalize();
            Ok(H160::from_slice(&result[12..]))
        }
        API::Zilliqa => {
            let mut hasher = Sha256::new();
            hasher.update(pubkey);
            let result = hasher.finalize();
            // Lop off the first 12 bytes.
            Ok(H160::from_slice(&result[12..]))
        }
    }
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
    let zilliqa_data_points: Vec<(&str, &str)> = vec![
        (
            "0246E7178DC8253201101E18FD6F6EB9972451D121FC57AA2A06DD5C111E58DC6A",
            "9BFEC715a6bD658fCb62B0f8cc9BFa2ADE71434A",
        ),
        (
            "02c261017f4299f0e60d33035d7fbc4b85cbaa11cd7127e0a420b331cd805e517a",
            "acd9339df14af808af1f46a3edb7466590199ee6",
        ),
    ];

    let ethereum_data_points: Vec<(&str, &str)> = vec![(
        "0293386b38b31fe37175eb42ddde0147b8154c584d80f9a33cf9d1558a82455358",
        "0x2d3ec573a07b101656847aa109be7ba5ed8dfe5d",
    )];
    for (pubkey, addr) in zilliqa_data_points {
        let dec_addr =
            address_from_public_key(&hex::decode(pubkey).unwrap(), API::Zilliqa).unwrap();
        let hex_addr = hex::encode(dec_addr);
        assert_eq!(hex_addr.to_lowercase(), addr.to_lowercase());
    }

    for (pubkey, addr) in ethereum_data_points {
        let dec_addr =
            address_from_public_key(&hex::decode(pubkey).unwrap(), API::Ethereum).unwrap();
        let hex_addr = hex::encode(dec_addr);
        assert_eq!(hex_addr.to_lowercase(), addr.to_lowercase());
    }
}
