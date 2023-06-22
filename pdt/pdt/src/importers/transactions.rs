// Bigquery importer.
use crate::importer::Importer;
#[allow(unused_imports)]
use anyhow::{anyhow, Result};
use async_trait::async_trait;
use pdtbq::bq::{Inserter, ZilliqaBQProject};
use pdtbq::values;
use pdtlib::exporter::Exporter;
use std::ops::Range;

/// We import transactions and microblocks at the same time because they use the same db.
pub struct TransactionMicroblockImporter {
    txn_inserter: Option<Inserter<values::Transaction>>,
    mb_inserter: Option<Inserter<values::Microblock>>,
    range: Option<Range<i64>>,
    client_id: String,
}

impl TransactionMicroblockImporter {
    pub fn new() -> Self {
        Self {
            txn_inserter: None,
            mb_inserter: None,
            range: None,
            client_id: "unset".to_string(),
        }
    }
}

#[async_trait]
impl Importer for TransactionMicroblockImporter {
    fn get_id(&self) -> String {
        "m".to_string()
    }

    fn set_client_id(&mut self, client_id: &str) {
        self.client_id = client_id.to_string();
    }

    async fn get_max_block(&self, exp: &Exporter) -> Result<i64> {
        Ok(exp.get_max_block().try_into()?)
    }

    async fn maybe_range(
        &self,
        project: &ZilliqaBQProject,
        last_max: i64,
    ) -> Result<Option<Range<i64>>> {
        Ok(project.get_txn_range(last_max).await?)
    }

    /// Set up an internal buffer.
    async fn extract_start(
        &mut self,
        project: &ZilliqaBQProject,
        _exporter: &Exporter,
    ) -> Result<()> {
        self.txn_inserter = Some(project.make_inserter::<values::Transaction>().await?);
        self.mb_inserter = Some(project.make_inserter::<values::Microblock>().await?);
        self.range = None;
        Ok(())
    }

    /// Extract a range from the database to an internal buffer.
    async fn extract_range(
        &mut self,
        _project: &ZilliqaBQProject,
        exporter: &Exporter,
        range: &Range<i64>,
    ) -> Result<()> {
        if let Some(_) = self.range {
            return Err(anyhow!(
                "Attempt to call extract_range when range is {:?} and not None",
                range
            ));
        }
        let mut nr_txns = 0;
        for val in exporter.micro_blocks(range.end.try_into()?, Some(range.start.try_into()?))? {
            if let Ok((key, blk)) = val {
                let mut offset_in_block = 0;
                if let Some(mb_inserter) = self.mb_inserter.as_mut() {
                    mb_inserter.insert_row(values::Microblock::from_proto(
                        &blk.clone(),
                        <i64>::try_from(key.epochnum)?,
                        <i64>::try_from(key.shardid)?,
                    )?)?;
                }
                // println!("Blk! at {}/{}", key.epochnum, key.shardid);
                for (_hash, maybe_txn) in exporter.txns(&key, &blk)? {
                    // println!("hash {}", _hash);
                    if let Some(txn) = maybe_txn {
                        if let Some(inserter) = self.txn_inserter.as_mut() {
                            inserter.insert_row(values::Transaction::from_proto(
                                &txn.clone(),
                                <i64>::try_from(key.epochnum)?,
                                offset_in_block,
                                <i64>::try_from(key.shardid)?,
                            )?)?;
                        }
                        nr_txns += 1;
                        offset_in_block += 1;
                        if let Some(_actually_txn) = txn.transaction {
                            // println!("Txn {}", H256::from_slice(&actually_txn.tranid));
                        }
                    }
                }
            }
        }
        self.range = Some(range.clone());
        println!("{}: accumulated {} txns", self.client_id, nr_txns);
        Ok(())
    }

    /// Insert the internal buffer into the database
    async fn extract_done(
        &mut self,
        project: &ZilliqaBQProject,
        _exporter: &Exporter,
    ) -> Result<()> {
        if let Some(range) = &self.range {
            if let Some(mb_inserter) = self.mb_inserter.take() {
                match project.insert_microblocks(mb_inserter, range).await {
                    Err(val) => {
                        println!("{}: Cannot import microblocks! {:}", self.client_id, val);
                    }
                    Ok(_) => (),
                }
            }
            if let Some(txn_inserter) = self.txn_inserter.take() {
                match project.insert_transactions(txn_inserter, range).await {
                    Err(val) => println!("{}: Errors {:}", self.client_id, val),
                    Ok(_) => (),
                };
            }
        } else {
            println!("{}: No data to export to database", self.client_id);
        }
        Ok(())
    }
}
