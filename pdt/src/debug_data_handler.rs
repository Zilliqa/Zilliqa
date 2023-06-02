// Implements a debugging data handler that just prints things.
use crate::data_handler;
use crate::proto::{ProtoMicroBlock, ProtoMicroBlockKey};
use anyhow::Result;

pub struct DebugDataHandler {}

impl DebugDataHandler {
    pub fn new() -> Result<Self> {
        Ok(DebugDataHandler {})
    }
}

pub impl DataHandler for DebugDataHandler {
    fn found_micro_block(&self, a_key: &ProtoMicroBlockKey) {
        println!("Found microblock");
    }

    fn found_txn(&self, a_txn: &ProtocolTransactionWithReceipt) -> Result<()> {}

    fn missing_txn(&self, hash: &H256, epoch_id: u64, shard_id: Option<u64>) -> Result<()> {}

    fn log(&self, level: LogLevel, msg: &str) -> Result<()> {}
}
