// Exports data from persistence
use crate::proto::{ProtoMicroBlock, ProtoMicroBlockKey, ProtoTransactionWithReceipt};
use crate::{db, db::Db, meta, proto};
use anyhow::Result;
use primitive_types::H256;
use std::path::Path;

// The maximum number of shards there can ever be.
const MAX_SHARDS: u8 = 16;

pub struct Exporter {
    #[allow(dead_code)]
    persistence_dir: String,
    db: Db,
    meta: meta::Meta,
}

impl Exporter {
    pub fn new(persistence_dir: &str) -> Result<Exporter> {
        let the_db = Db::new(persistence_dir)?;
        let meta = meta::Meta::load(Path::new(persistence_dir))?;
        Ok(Exporter {
            persistence_dir: persistence_dir.to_string(),
            db: the_db,
            meta,
        })
    }

    pub fn get_max_block(&self) -> u64 {
        self.meta.max_block()
    }

    pub fn micro_blocks(&self, end_blk: u64, start_at: Option<u64>) -> Result<MicroBlocks> {
        let lower_blk_id: u64 = if let Some(x) = start_at { x } else { 0 };
        Ok(MicroBlocks {
            end_blk,
            blk_id: lower_blk_id,
            first_block: true,
            shard: 0,
            shard_structure: Vec::new(),
            db: &self.db,
        })
    }

    pub fn txns<'a>(
        &'a self,
        key: &ProtoMicroBlockKey,
        mb: &'a ProtoMicroBlock,
    ) -> Result<Box<dyn Iterator<Item = (H256, Option<ProtoTransactionWithReceipt>)> + 'a>> {
        let blk_id = key.epochnum;
        Ok(Box::new(mb.tranhashes.iter().map(move |x| {
            let hash = H256::from_slice(&x);
            if let Ok(maybe_txn) = self.db.get_tx_body(blk_id, hash) {
                (hash, maybe_txn)
            } else {
                (hash, None)
            }
        })))
    }
}

pub struct MicroBlocks<'a> {
    end_blk: u64,
    blk_id: u64,
    first_block: bool,
    shard: usize,
    shard_structure: Vec<u8>,
    db: &'a Db,
}

impl<'a> Iterator for MicroBlocks<'a> {
    type Item = Result<(ProtoMicroBlockKey, ProtoMicroBlock)>;

    /// At the start of time, set the shard structure to empty and blk_id to lower_blk_id-1
    /// shard will be incremented, which will cause us to increment blk_id and fault in the
    /// shard structure for the next block.
    fn next(&mut self) -> Option<Result<(ProtoMicroBlockKey, ProtoMicroBlock)>> {
        loop {
            self.shard += 1;
            if self.shard >= self.shard_structure.len() {
                // Next block!
                if self.first_block {
                    self.first_block = false;
                } else {
                    self.blk_id += 1;
                }
                if self.blk_id >= self.end_blk {
                    return None;
                }
                self.shard = 0;
                match self.db.get_shard_structure(self.blk_id) {
                    Ok(val) => {
                        if let Some(structure) = val {
                            if structure.len() > 0 {
                                self.shard_structure = structure.clone();
                            } else {
                                continue;
                            }
                        } else {
                            // No shard structure for this block :-(
                            // println!("no shard structure for blk {}", self.blk_id);
                            self.shard_structure = Vec::new();
                            for i in 0..MAX_SHARDS {
                                self.shard_structure.push(i);
                            }
                        }
                    }
                    Err(val) => {
                        return Some(Err(val));
                    }
                };
            }
            let the_key = ProtoMicroBlockKey {
                epochnum: self.blk_id,
                shardid: self.shard_structure[self.shard].into(),
            };
            match self.db.get_micro_block(&the_key) {
                Ok(val) => {
                    if let Some(blk) = val {
                        return Some(Ok((the_key, blk)));
                    }
                    // println!("No microblock at {}/{}", self.blk_id, self.shard);
                }
                Err(val) => {
                    println!("Microblock {} {} err {:?}", self.blk_id, self.shard, val);
                    return Some(Err(val));
                }
            };
            // Otherwise go round and get the next one, if it exists.
        }
    }
}
