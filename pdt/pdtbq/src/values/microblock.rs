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
pub struct Microblock {
    pub block: i64,
    pub offset_in_block: i64,
    // The shard id from the index.
    pub shard_id: i64,
    pub header_version: i64,
    pub header_committee_hash: Option<String>,
    pub header_prev_hash: Option<String>,
    pub gas_limit: i64,
    // Rewards.
    pub rewards: Option<String>,
    pub prev_hash: Option<String>,
    pub tx_root_hash: Option<String>,
    pub miner_pubkey: Option<String>,
    pub miner_addr_zil: Option<String>,
    pub miner_addr_eth: Option<String>,
    pub ds_block_num: i64,
    pub state_delta_hash: Option<String>,
    pub tran_receipt_hash: Option<String>,
    pub block_shard_id: i64,
    pub gas_used: i64,
    pub epoch_num: i64,
    pub num_txs: i64,
    pub blockhash: Option<String>,
    pub timestamp: i64,
    pub cs1: Option<String>,
    // represented as a string of '0' or '1' for easier querying.
    pub b1: Option<String>,
    pub cs2: Option<String>,
    // represented as a string of '0' or '1' for easier querying.
    pub b2: Option<String>,
}

impl bq::BlockInsertable for Microblock {
    fn get_coords(&self) -> (i64, i64) {
        (self.block, 0)
    }

    fn estimate_bytes(&self) -> Result<usize> {
        Ok(self.to_json()?.len())
    }
}

impl Microblock {
    pub fn from_proto(in_val: &ProtoMicroBlock, blk: i64, shard_id: i64) -> Result<Self> {
        let val = in_val.clone();
        let header = val
            .header
            .as_ref()
            .ok_or(anyhow!("Couldn't find block header"))?;
        let header_base = header
            .blockheaderbase
            .as_ref()
            .ok_or(anyhow!("Block header base"))?;
        let block_base = val.blockbase.as_ref().ok_or(anyhow!("Block base"))?;
        let cosigs = block_base.cosigs.as_ref().ok_or(anyhow!("No cosigs"))?;

        let header_committee_hash = Some(encode_u8(header_base.committeehash.as_slice()));
        let header_prev_hash = Some(encode_u8(header_base.prevhash.as_slice()));
        let rewards = header
            .rewards
            .clone()
            .and_then(|x| utils::u128_string_from_storage(&x));
        let prev_hash = Some(encode_u8(header.prevhash.as_slice()));
        let tx_root_hash = Some(encode_u8(header.txroothash.as_slice()));
        let miner_pubkey = header
            .minerpubkey
            .clone()
            .and_then(|x| Some(encode_u8(x.data.as_slice())));
        let miner_addr_zil = header
            .minerpubkey
            .as_ref()
            .and_then(|x| utils::maybe_hex_address_from_public_key(&x.data, utils::API::Zilliqa));
        let miner_addr_eth = header
            .minerpubkey
            .as_ref()
            .and_then(|x| utils::maybe_hex_address_from_public_key(&x.data, utils::API::Ethereum));
        let state_delta_hash = Some(encode_u8(header.statedeltahash.as_slice()));
        let tran_receipt_hash = Some(encode_u8(header.tranreceipthash.as_slice()));
        let block_shard_id: i64 = header.oneof2.as_ref().map_or(-1, |x| {
            let pdtlib::proto::proto_micro_block::micro_block_header::Oneof2::Shardid(val) = x;
            i64::try_from(*val).unwrap_or(-1)
        });
        let gas_used = header.oneof4.as_ref().map_or(-1, |x| {
            let pdtlib::proto::proto_micro_block::micro_block_header::Oneof4::Gasused(val) = x;
            i64::try_from(*val).unwrap_or(-1)
        });
        let epoch_num = header.oneof7.as_ref().map_or(-1, |x| {
            let pdtlib::proto::proto_micro_block::micro_block_header::Oneof7::Epochnum(val) = x;
            i64::try_from(*val).unwrap_or(-1)
        });
        let num_txs = header.oneof9.as_ref().map_or(-1, |x| {
            let pdtlib::proto::proto_micro_block::micro_block_header::Oneof9::Numtxs(val) = x;
            i64::try_from(*val).unwrap_or(-1)
        });

        let blockhash = Some(encode_u8(block_base.blockhash.as_slice()));
        let cs1 = cosigs
            .cs1
            .clone()
            .and_then(|x| Some(encode_u8(x.data.as_slice())));
        let cs2 = cosigs
            .cs2
            .clone()
            .and_then(|x| Some(encode_u8(x.data.as_slice())));
        let b1: String = cosigs
            .b1
            .iter()
            .map(|x| if *x { '1' } else { '0' })
            .collect();
        let b2: String = cosigs
            .b2
            .iter()
            .map(|x| if *x { '1' } else { '0' })
            .collect();

        Ok(Self {
            block: blk,
            offset_in_block: 0,
            shard_id,
            header_version: header_base.version.into(),
            header_committee_hash,
            header_prev_hash,
            gas_limit: header.gaslimit.try_into()?,
            rewards,
            prev_hash,
            tx_root_hash,
            miner_pubkey,
            miner_addr_zil,
            miner_addr_eth,
            ds_block_num: header.dsblocknum.try_into()?,
            state_delta_hash,
            tran_receipt_hash,
            block_shard_id,
            gas_used,
            epoch_num,
            num_txs,
            blockhash,
            timestamp: block_base.timestamp.try_into()?,
            cs1,
            b1: Some(b1),
            cs2,
            b2: Some(b2),
        })
    }

    pub fn to_json(&self) -> Result<String> {
        Ok(serde_json::to_string(self)?)
    }
}

fn encode_u8(y: &[u8]) -> String {
    base64::engine::general_purpose::STANDARD
        .encode(y)
        .to_string()
}
