// Exports data from persistence
use crate::proto::{ProtoMicroBlock, ProtoMicroBlockKey};
use crate::{db, db::Db, proto};
use anyhow::Result;
use primitive_types::H256;

pub struct Exporter {
    persistence_dir: String,
    db: Db,
}

impl Exporter {
    pub fn new(persistence_dir: &str) -> Result<Exporter> {
        let the_db = Db::new(persistence_dir)?;
        Ok(Exporter {
            persistence_dir: persistence_dir.to_string(),
            db: the_db,
        })
    }

    // this is a bit weird, because only some elements of history
    // are preserved.
    pub fn import_txns(
        &self,
        nr_shards: u32,
        nr_blks: u64,
        start_after: Option<u64>,
    ) -> Result<()> {
        // Start at epoch 1.
        let lower_blk_id: u64 = if let Some(x) = start_after { x } else { 0 };
        for blk_id in lower_blk_id..nr_blks {
            for shard in 0..nr_shards {
                let the_key = ProtoMicroBlockKey {
                    epochnum: blk_id,
                    shardid: shard,
                };
                if let Some(block) = self.db.get_micro_block(&the_key)? {
                    println!(
                        "Got block {}, shard {} hashes {}",
                        blk_id,
                        shard,
                        block.tranhashes.len()
                    );
                    // retrieve the transaction hashes
                    for hash_bytes in block.tranhashes {
                        let hash = H256::from_slice(&hash_bytes);
                        if let Ok(maybe_txn) = self.db.get_tx_body(blk_id, hash) {
                            if let Some(txn) = maybe_txn {
                                if let Some(pt) = txn.transaction {
                                    println!("Got txn {:?}", pt.tranid);
                                }
                            }
                        }
                    }
                }
            }
        }
        Ok(())
    }
}
