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
pub struct DSVoteDS {
    pub block: i64,
    pub offset_in_block: i64,
    pub key: u32,
    pub dsvotes_vote_value: u32,
    pub dsvotes_vote_count: u32,
}

#[derive(Serialize, Deserialize, Clone)]
pub struct DSVoteShard {
    pub block: i64,
    pub offset_in_block: i64,
    pub key: u32,
    pub shard_vote_value: u32,
    pub shard_vote_count: u32,
}
