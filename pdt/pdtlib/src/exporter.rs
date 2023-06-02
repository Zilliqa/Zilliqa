// Exports data from persistence
use crate::proto::{ProtoMicroBlock, ProtoMicroBlockKey, ProtoTransactionWithReceipt};
use crate::{db, db::Db, meta, proto};
use anyhow::Result;
use primitive_types::H256;
use std::path::Path;

pub struct Exporter {
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

    pub fn micro_blocks(
        &self,
        nr_shards: u32,
        nr_blks: u64,
        start_after: Option<u64>,
    ) -> Result<MicroBlocks> {
        let lower_blk_id: u64 = if let Some(x) = start_after { x } else { 0 };
        Ok(MicroBlocks {
            nr_shards,
            nr_blks,
            blk_id: lower_blk_id,
            shard: 0,
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
    nr_shards: u32,
    nr_blks: u64,
    blk_id: u64,
    shard: u32,
    db: &'a Db,
}

impl<'a> Iterator for MicroBlocks<'a> {
    type Item = Result<(ProtoMicroBlockKey, ProtoMicroBlock)>;

    fn next(&mut self) -> Option<Result<(ProtoMicroBlockKey, ProtoMicroBlock)>> {
        loop {
            self.shard += 1;
            if self.shard >= self.nr_shards {
                self.shard = 0;
                self.blk_id += 1;
            }
            if self.blk_id >= self.nr_blks {
                return None;
            }
            let the_key = ProtoMicroBlockKey {
                epochnum: self.blk_id,
                shardid: self.shard,
            };
            match self.db.get_micro_block(&the_key) {
                Ok(val) => {
                    if let Some(blk) = val {
                        return Some(Ok((the_key, blk)));
                    }
                    // println!("No microblock at {}/{}", self.blk_id, self.shard;)
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
// this is a bit weird, because only some elements of history
// are preserved.
// Start at epoch 1.
// We're done.
// loop {

//     if self.blk_id >= self.nr_blks {
//         return None;
//     }
//     if self.shard >= self.nr_shards {
//         self.shard = 0;
//     }
//     let the_key = ProtoMicroBlockKey {
//         epochnum: blk_id,
//         shardid: shard,
//     };
//     if let Some(block) = self.db.get_micro_block(&the_key)? {
//         println!(
//             "Got block {}, shard {} hashes {}",
//             blk_id,
//             shard,
//             block.tranhashes.len()
//         );
//         // retrieve the transaction hashes
//         for hash_bytes in block.tranhashes {
//             let hash = H256::from_slice(&hash_bytes);
//             if let Ok(maybe_txn) = self.db.get_tx_body(blk_id, hash) {
//                 if let Some(txn) = maybe_txn {
//                     if let Some(pt) = txn.transaction {
//                             println!("Got txn {:?}", pt.tranid);
//                         }
//                     }
//                 }
//             }
//         }
//     }
// }
