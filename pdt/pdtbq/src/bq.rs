use crate::bq_object;
use crate::meta::Meta;
#[allow(unused_imports)]
use anyhow::{anyhow, Result};
use gcp_bigquery_client::model::{
    dataset::Dataset, query_request::QueryRequest, range_partitioning::RangePartitioning,
    range_partitioning_range::RangePartitioningRange, table::Table,
    table_data_insert_all_request::TableDataInsertAllRequest, table_field_schema::TableFieldSchema,
    table_schema::TableSchema,
};
use gcp_bigquery_client::Client;
use serde::Serialize;
use std::fmt;
use std::marker::PhantomData;
use std::ops::Range;
use tokio::time::{sleep, Duration};

pub const DATASET_ID: &str = "zilliqa";
pub const TRANSACTION_TABLE_ID: &str = "transactions";
pub const METADATA_TABLE_ID: &str = "meta";

// BigQuery imposes a limit of 10MiB per HTTP request.
pub const MAX_QUERY_BYTES: usize = 9 << 20;

// BigQuery imposes a 50k row limit and recommends 500
pub const MAX_QUERY_ROWS: usize = 500;

pub struct InsertionErrors {
    pub errors: Vec<String>,
    pub msg: String,
}

impl fmt::Display for InsertionErrors {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}\n {:?}", self.msg, self.errors)
    }
}

impl InsertionErrors {
    pub fn from_msg(msg: &str) -> Self {
        InsertionErrors {
            errors: Vec::new(),
            msg: msg.to_string(),
        }
    }
}

pub struct ZilliqaBQProject {
    pub project_id: String,
    pub bq_client: Client,
    pub ds: Dataset,
    pub transactions: Table,
    pub meta: Meta,
    pub transaction_table_name: String,
    pub client_id: String,
}

pub struct Inserter<T: Serialize> {
    _marker: PhantomData<T>,
    req: Vec<T>, //req: Vec<TableDataInsertAllRequest>,
}

impl<T: Serialize> Inserter<T> {
    pub fn insert_row(&mut self, row: T) -> Result<()> {
        self.req.push(row);
        Ok(())
    }
}

impl ZilliqaBQProject {
    pub async fn new(
        project_id: &str,
        service_account_key_file: &str,
        nr_machines: i64,
        nr_blks: i64,
        batch_blks: i64,
        machine_id: i64,
        client_id: &str,
    ) -> Result<Self> {
        // Application default creds don't work here because the auth library looks for a service
        // account key in the file you give it and, of course, it's not there ..
        let my_client = Client::from_service_account_key_file(service_account_key_file).await?;
        let zq_ds = if let Ok(ds) = my_client.dataset().get(project_id, DATASET_ID).await {
            ds
        } else {
            my_client
                .dataset()
                .create(Dataset::new(project_id, DATASET_ID))
                .await?
        };
        let transaction_table = if let Ok(tbl) = my_client
            .table()
            .get(project_id, DATASET_ID, TRANSACTION_TABLE_ID, Option::None)
            .await
        {
            tbl
        } else {
            Self::create_transaction_table(&my_client, project_id).await?
        };

        let meta = Meta::new(
            project_id,
            nr_machines,
            nr_blks,
            batch_blks,
            machine_id,
            client_id,
        );

        meta.ensure_table(&my_client).await?;

        let ttn = format!("{}.{}.{}", project_id, DATASET_ID, TRANSACTION_TABLE_ID);
        Ok(ZilliqaBQProject {
            project_id: project_id.to_string(),
            bq_client: my_client,
            ds: zq_ds,
            transactions: transaction_table,
            meta,
            transaction_table_name: ttn,
            client_id: client_id.to_string(),
        })
    }

    /// Get the max block
    pub async fn get_max_block(&self) -> i64 {
        self.meta.get_nr_blocks() - 1
    }

    /// Create an insertion request
    pub async fn make_transaction_inserter(&self) -> Result<Inserter<bq_object::Transaction>> {
        Ok(Inserter {
            _marker: PhantomData,
            req: Vec::new(),
        })
    }

    /// Act on an inserter.
    pub async fn insert_transactions(
        &self,
        req: Inserter<bq_object::Transaction>,
        blks: &Range<i64>,
    ) -> Result<(), InsertionErrors> {
        let _txn_table_name = format!(
            "{}.{}.{}",
            &self.project_id, DATASET_ID, TRANSACTION_TABLE_ID
        );
        // This is a bit horrid. If there is any action at all, we need to check what the highest txn
        // we successfully inserted was.
        let mut last_blk: i64 = -1;
        let mut last_txn: i64 = -1;
        if req.req.len() > 1 {
            let last_blks = self.get_last_txn_for_blocks(blks).await;
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
            project_id: &str,
            req: TableDataInsertAllRequest,
        ) -> Result<(), InsertionErrors> {
            let mut err_rows = Vec::<String>::new();
            let resp = client
                .tabledata()
                .insert_all(&project_id, DATASET_ID, TRANSACTION_TABLE_ID, req)
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
            if txn.block > last_blk || (txn.block == last_blk && txn.offset_in_block > last_txn) {
                let nr_bytes = match txn.estimate_bytes() {
                    Ok(val) => val,
                    Err(x) => {
                        return Err(InsertionErrors::from_msg(&format!(
                            "Cannot get size of transaction - {}",
                            x
                        )))
                    }
                };

                if current_request_bytes + nr_bytes >= MAX_QUERY_BYTES
                    || current_request.len() >= MAX_QUERY_ROWS - 1
                {
                    println!(
                        "{}: Inserting {} rows with {} bytes ending at {}/{}",
                        self.client_id,
                        current_request.len(),
                        current_request_bytes,
                        txn.block,
                        txn.offset_in_block
                    );

                    commit_request(&self.bq_client, &self.project_id, current_request).await?;
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
                self.client_id,
                current_request.len(),
            );
            commit_request(&self.bq_client, &self.project_id, current_request).await?;
        }

        // Mark that these blocks were done.
        if let Err(e) = self.meta.commit_run(&self.bq_client, &blks).await {
            return Err(InsertionErrors::from_msg(&format!(
                "Could not commit run result - {:?}",
                e
            )));
        }
        Ok(())
    }

    // Retrieve the last (blk,txnid) pair for the blocks in the range, so we can avoid inserting duplicates.
    // Since these blocks are assigned to only one thread at a time, we know another thread can't try to insert
    // them concurrently - but we might have crashed half-way through a block of insert requests earlier.
    async fn get_last_txn_for_blocks(&self, blks: &Range<i64>) -> Result<(i64, i64)> {
        let mut result = self.bq_client.job()
            .query(&self.project_id,
                   QueryRequest::new(format!("SELECT block,offset_in_block FROM {} WHERE block >= {} AND block < {} ORDER BY block DESC, offset_in_block DESC LIMIT 1",
                                             self.transaction_table_name, blks.start, blks.end))).await?;
        if result.next_row() {
            // There was one!1
            let blk = result.get_i64(0)?.ok_or(anyhow!("Cannot decode blk"))?;
            let offset = result.get_i64(1)?.ok_or(anyhow!("Cannot decode offset"))?;
            Ok((blk, offset))
        } else {
            Ok((-1, -1))
        }
    }

    pub async fn get_range(&self, start_at: i64) -> Result<Option<Range<i64>>> {
        Ok(self
            .meta
            .find_next_range_to_do(&self.bq_client, start_at)
            .await?)
    }

    pub fn get_max_insert_bytes(&self) -> usize {
        MAX_QUERY_BYTES
    }

    pub fn get_max_query_rows(&self) -> usize {
        MAX_QUERY_ROWS
    }

    /// If a single entry in the meta table contains start  .. start+blks , then return the client id
    /// that generated it, else return None.
    /// Returns a pair (nr_blks, client_id)
    pub async fn is_range_covered_by_entry(
        &self,
        start: i64,
        blks: i64,
    ) -> Result<Option<(i64, String)>> {
        self.meta
            .is_range_covered_by_entry(&self.bq_client, start, blks)
            .await
    }

    async fn create_transaction_table(client: &Client, project_id: &str) -> Result<Table> {
        let transaction_table = Table::new(
            project_id,
            DATASET_ID,
            TRANSACTION_TABLE_ID,
            TableSchema::new(vec![
                TableFieldSchema::string("id"),
                TableFieldSchema::integer("block"),
                TableFieldSchema::integer("offset_in_block"),
                TableFieldSchema::big_numeric("amount"),
                TableFieldSchema::string("code"),
                TableFieldSchema::string("data"),
                TableFieldSchema::string("code_base64"),
                TableFieldSchema::string("data_base64"),
                TableFieldSchema::integer("gas_limit"),
                TableFieldSchema::big_numeric("gas_price"),
                TableFieldSchema::integer("nonce"),
                TableFieldSchema::string("receipt"),
                TableFieldSchema::string("sender_public_key"),
                TableFieldSchema::string("from_addr"),
                TableFieldSchema::string("signature"),
                TableFieldSchema::string("to_addr"),
                TableFieldSchema::integer("version"),
                TableFieldSchema::integer("cum_gas"),
                TableFieldSchema::integer("shard_id"),
            ]),
        )
        .range_partitioning(RangePartitioning {
            field: Some("block".to_string()),
            range: Some(RangePartitioningRange {
                start: "0".to_string(),
                // There can only be 10k partitions.
                // Make it 10k less than that.
                end: "100000000".to_string(),
                // About once every two weeks?
                interval: "100000".to_string(),
            }),
        });
        let the_table = client.table().create(transaction_table).await;
        match the_table {
            Ok(tbl) => Ok(tbl),
            Err(_) => {
                // Wait a bit and then fetch the table.
                sleep(Duration::from_millis(5_000)).await;
                Ok(client
                    .table()
                    .get(project_id, DATASET_ID, TRANSACTION_TABLE_ID, Option::None)
                    .await?)
            }
        }
    }
}
