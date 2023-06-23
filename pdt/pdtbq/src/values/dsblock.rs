use crate::bq;
use crate::utils;
use anyhow::{anyhow, Result};
use base64::Engine;
// use hex;
use pdtlib::proto::ProtoMicroBlock;
//use pdtlib::proto::ProtoTransactionWithReceipt;
// use primitive_types::{H160, H256};
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Clone)]
pub struct DSBlock {
    pub block: i64,
    pub offset_in_block: i64,
    // DsBlockHeader
    // ProtoBlockHeaderBase
    pub header_version: u32,
    pub header_committee_hash: Option<String>,
    pub header_prev_hash: Option<String>,
    // Main
    pub ds_difficulty: u32,
    pub difficulty: u32,
    pub prevhash: Option<String>,
    pub leaderpubkey: Option<String>,
    // Number
    pub gasprice: Option<String>,
    // SWInfo
    pub swinfo_major: Option<u32>,
    pub swinfo_minor: Option<u32>,
    pub swinfo_fix: Option<u32>,
    pub swinfo_upgrade_ds: Option<u64>,
    pub swinfo_commit: Option<u32>,
    pub swinfo_scilla_major: Option<u32>,
    pub swinfo_scilla_minor: Option<u32>,
    pub swinfo_scilla_fix: Option<u32>,
    pub swinfo_scilla_upgrade_ds: Option<u64>,
    pub swinfo_scilla_commit: Option<u32>,
    pub swinfo_raw: Option<String>,
    // DS winners is in the dswinners table
    pub hash_sharding_hash: Option<String>,
    pub hash_reserved: Option<String>,
    // DS removed is in the dsremoved table
    // proposals are in the ds_votes_ds and ds_votes_shard table
    pub block_num: i64,
    pub epoch_num: i64,
}

impl bq::BlockInsertable for DSBlock {
    fn get_coords(&self) -> (i64, i64) {
        (self.block, self.offset_in_block)
    }

    fn estimate_bytes(&self) -> Result<usize> {
        Ok(self.to_json()?.len())
    }
}

impl DSBlock {
    pub fn from_proto(in_val: &DsBlockHeader, blk: i64) -> Result<Self> {
        let base = in_val
            .blockheaderbase
            .as_ref()
            .ok_or(anyhow!("No block header base"));
        let header_committee_hash = Some(base.encode_u8(base.committeehash.as_slice()));

        Ok(Self {
            block: blk,
            offset_in_block: 0,
            header_version: base.version,
            header_committee_hash,
        })
    }
}
