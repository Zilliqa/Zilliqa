// A trait which handles data from the exporters.
use crate::proto::{ProtoMicroBlock, ProtoMicroBlockKey};
use anyhow::Result;

pub enum LogLevel {
    Info,
    Warn,
    Err,
}

pub trait DataHandler {
    // Log a message
    fn log(&self, level: LogLevel, msg: &str) -> Result<()>;

    // Called when we found a microblock.
    fn found_micro_block(&self, a_key: &ProtoMicroBlockKey) -> Result<()>;
    // Called when we found a transaction.
    fn found_txn(&self, a_txn: &ProtoTransactionWithReceipt) -> Result<()>;

    // called when we couldn't find a transaction
    fn missing_txn(&self, hash: &H256, epoch_id: u64, shard_id: Option<u64>) -> Result<()>;
}
