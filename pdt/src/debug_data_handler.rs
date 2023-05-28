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
    }
    
    fn found_txn(&self, a_txn: &ProtocolTran
}
