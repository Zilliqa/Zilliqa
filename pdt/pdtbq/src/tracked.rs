use crate::bq;
use crate::bq::InsertionErrors;
use crate::meta::Meta;
use crate::utils;
#[allow(unused_imports)]
use anyhow::{anyhow, Result};
use gcp_bigquery_client::model::{
    query_request::QueryRequest, table::Table,
    table_data_insert_all_request::TableDataInsertAllRequest,
};
use gcp_bigquery_client::Client;
use serde::Serialize;
use std::ops::Range;

pub struct TrackedTable {
    pub location: utils::BigQueryTableLocation,
    pub table: Table,
    pub meta: Meta,
}

impl TrackedTable {
    pub async fn ensure_schema(
        client: &Client,
        location: &utils::BigQueryTableLocation,
    ) -> Result<()> {
        Ok(Meta::ensure_schema(client, &location.to_meta()).await?)
    }

    pub fn new(
        location: &utils::BigQueryTableLocation,
        coords: &utils::ProcessCoordinates,
        table: Table,
        nr_blks: i64,
    ) -> Result<Self> {
        let meta = Meta::new(&location.to_meta(), coords, nr_blks)?;
        Ok(TrackedTable {
            location: location.clone(),
            table,
            meta,
        })
    }

    // Retrieve the last (blk,txnid) pair for the blocks in the range, so we can avoid inserting duplicates.
    // Since these blocks are assigned to only one thread at a time, we know another thread can't try to insert
    // them concurrently - but we might have crashed half-way through a block of insert requests earlier.
    async fn get_last_txn_for_blocks(
        &self,
        client: &Client,
        blks: &Range<i64>,
    ) -> Result<(i64, i64)> {
        let mut result = client.job()
            .query(&self.location.dataset.project_id,
                   QueryRequest::new(format!("SELECT block,offset_in_block FROM {} WHERE block >= {} AND block < {} ORDER BY block DESC, offset_in_block DESC LIMIT 1",
                                             self.location.get_table_desc(), blks.start, blks.end))).await?;
        if result.next_row() {
            // There was one!1
            let blk = result.get_i64(0)?.ok_or(anyhow!("Cannot decode blk"))?;
            let offset = result.get_i64(1)?.ok_or(anyhow!("Cannot decode offset"))?;
            Ok((blk, offset))
        } else {
            Ok((-1, -1))
        }
    }

    pub async fn insert<T: Serialize + bq::BlockInsertable>(
        &self,
        client: &Client,
        req: bq::Inserter<T>,
        blks: &Range<i64>,
    ) -> Result<(), InsertionErrors> {
        let _txn_table_name = self.location.get_table_desc();

        // This is a bit horrid. If there is any action at all, we need to check what the highest txn
        // we successfully inserted was.
        let mut last_blk: i64 = -1;
        let mut last_txn: i64 = -1;
        if req.req.len() > 1 {
            let last_blks = self.get_last_txn_for_blocks(client, blks).await;
            match last_blks {
                Ok((a, b)) => (last_blk, last_txn) = (a, b),
                Err(x) => {
                    return Err(InsertionErrors::from_msg(&format!(
                        "Cannot find inserted txn ids - {}",
                        x
                    )));
                }
            }
        }

        async fn commit_request(
            client: &Client,
            loc: &utils::BigQueryTableLocation,
            req: TableDataInsertAllRequest,
        ) -> Result<(), InsertionErrors> {
            let mut err_rows = Vec::<String>::new();
            let resp = client
                .tabledata()
                .insert_all(
                    &loc.dataset.project_id,
                    &loc.dataset.dataset_id,
                    &loc.table_id,
                    req,
                )
                .await
                .or_else(|e| Err(InsertionErrors::from_msg(&format!("Cannot insert - {}", e))))?;
            if let Some(row_errors) = resp.insert_errors {
                if row_errors.len() > 0 {
                    for err in row_errors {
                        let err_string = format!("{:?}: {:?}", err.index, err.errors);
                        err_rows.push(err_string);
                    }
                }
            }
            if err_rows.len() > 0 {
                return Err(InsertionErrors {
                    errors: err_rows,
                    msg: "Insertion failed".to_string(),
                });
            }
            Ok(())
        }
        let mut current_request = TableDataInsertAllRequest::new();
        let mut current_request_bytes: usize = 0;

        for txn in req.req {
            let (txn_block, txn_offset_in_block) = txn.get_coords();
            if txn_block > last_blk || (txn_block == last_blk && txn_offset_in_block > last_txn) {
                let nr_bytes = match txn.estimate_bytes() {
                    Ok(val) => val,
                    Err(x) => {
                        return Err(InsertionErrors::from_msg(&format!(
                            "Cannot get size of transaction - {}",
                            x
                        )))
                    }
                };

                if current_request_bytes + nr_bytes >= bq::MAX_QUERY_BYTES
                    || current_request.len() >= bq::MAX_QUERY_ROWS - 1
                {
                    println!(
                        "{}: Inserting {} rows with {} bytes ending at {}/{}",
                        self.meta.coords.client_id,
                        current_request.len(),
                        current_request_bytes,
                        txn_block,
                        txn_offset_in_block
                    );

                    commit_request(client, &self.location, current_request).await?;
                    current_request = TableDataInsertAllRequest::new();
                    current_request_bytes = 0;
                }

                current_request_bytes += nr_bytes;
                // println!("T:{:?}", txn.to_json());
                match current_request.add_row(None, &txn) {
                    Ok(_) => (),
                    Err(x) => {
                        return Err(InsertionErrors::from_msg(&format!(
                            "Cannot add row to request - {}",
                            x
                        )));
                    }
                }
            }
        }
        if current_request.len() > 0 {
            println!(
                "{}: [F] Inserting {} rows at end of block",
                self.meta.coords.client_id,
                current_request.len(),
            );
            commit_request(&client, &self.location, current_request).await?;
        }

        // Mark that these blocks were done.
        if let Err(e) = self.meta.commit_run(&client, &blks).await {
            return Err(InsertionErrors::from_msg(&format!(
                "Could not commit run result - {:?}",
                e
            )));
        }
        Ok(())
    }

    pub async fn is_range_covered_by_entry(
        &self,
        client: &Client,
        start: i64,
        blks: i64,
    ) -> Result<Option<(i64, String)>> {
        self.meta
            .is_range_covered_by_entry(client, start, blks)
            .await
    }

    pub async fn find_next_range_to_do(
        &self,
        client: &Client,
        start_at: i64,
    ) -> Result<Option<Range<i64>>> {
        Ok(self.meta.find_next_range_to_do(client, start_at).await?)
    }
}
