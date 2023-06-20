use crate::tracked::TrackedTable;
use crate::utils;
use crate::values;
#[allow(unused_imports)]
use anyhow::{anyhow, Result};
#[allow(unused_imports)]
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

pub const TRANSACTION_TABLE_ID: &str = "transactions";
pub const MICROBLOCK_TABLE_ID: &str = "microblocks";

// BigQuery imposes a limit of 10MiB per HTTP request.
pub const MAX_QUERY_BYTES: usize = 9 << 20;

// BigQuery imposes a 50k row limit and recommends 500
pub const MAX_QUERY_ROWS: usize = 500;

pub trait BlockInsertable {
    /// Return (block, offset_in_block)
    fn get_coords(&self) -> (i64, i64);

    /// return an number >= the number of bytes used by this object so that
    /// we can make sure our bigquery request isn't too big.
    fn estimate_bytes(&self) -> Result<usize>;
}

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
    pub bq: utils::BigQueryDatasetLocation,
    pub bq_client: Client,
    pub ds: Dataset,
    pub transactions: TrackedTable,
    pub microblocks: TrackedTable,
    pub client_id: String,
    pub nr_blocks: i64,
}

pub struct Inserter<T: Serialize> {
    _marker: PhantomData<T>,
    pub(crate) req: Vec<T>, //req: Vec<TableDataInsertAllRequest>,
}

impl<T: Serialize> Inserter<T> {
    pub fn insert_row(&mut self, row: T) -> Result<()> {
        self.req.push(row);
        Ok(())
    }
}

impl ZilliqaBQProject {
    pub async fn new(
        bq: &utils::BigQueryDatasetLocation,
        coords: &utils::ProcessCoordinates,
        service_account_key_file: &str,
    ) -> Result<Self> {
        // Application default creds don't work here because the auth library looks for a service
        // account key in the file you give it and, of course, it's not there ..
        let my_client = Client::from_service_account_key_file(service_account_key_file).await?;
        let zq_ds = if let Ok(ds) = my_client
            .dataset()
            .get(&bq.project_id, &bq.dataset_id)
            .await
        {
            ds
        } else {
            my_client
                .dataset()
                .create(Dataset::new(&bq.project_id, &bq.dataset_id))
                .await?
        };
        let txn_location = utils::BigQueryTableLocation::new(&bq, TRANSACTION_TABLE_ID);
        let transaction_table = if let Ok(tbl) = my_client
            .table()
            .get(
                &bq.project_id,
                &bq.dataset_id,
                TRANSACTION_TABLE_ID,
                Option::None,
            )
            .await
        {
            tbl
        } else {
            Self::create_transaction_table(&my_client, &txn_location).await?
        };
        let txns = TrackedTable::new(&txn_location, coords, transaction_table)?;
        let microblock_location = utils::BigQueryTableLocation::new(&bq, MICROBLOCK_TABLE_ID);
        let microblock_table = if let Ok(tbl) = my_client
            .table()
            .get(
                &bq.project_id,
                &bq.dataset_id,
                MICROBLOCK_TABLE_ID,
                Option::None,
            )
            .await
        {
            tbl
        } else {
            Self::create_microblock_table(&my_client, &microblock_location).await?
        };
        let micros = TrackedTable::new(&microblock_location, coords, microblock_table)?;

        txns.ensure(&my_client).await?;
        micros.ensure(&my_client).await?;

        Ok(ZilliqaBQProject {
            bq: bq.clone(),
            bq_client: my_client,
            ds: zq_ds,
            transactions: txns,
            microblocks: micros,
            client_id: coords.client_id.to_string(),
            nr_blocks: coords.nr_blks,
        })
    }

    /// Get the max block
    pub async fn get_max_block(&self) -> i64 {
        self.nr_blocks - 1
    }

    /// Create an insertion request
    pub async fn make_inserter<T: Serialize + BlockInsertable>(&self) -> Result<Inserter<T>> {
        Ok(Inserter {
            _marker: PhantomData,
            req: Vec::new(),
        })
    }

    /// Act on an inserter.
    pub async fn insert_transactions(
        &self,
        req: Inserter<values::Transaction>,
        blks: &Range<i64>,
    ) -> Result<(), InsertionErrors> {
        Ok(self.transactions.insert(&self.bq_client, req, blks).await?)
    }

    /// Act on an inserter.
    pub async fn insert_microblocks(
        &self,
        req: Inserter<values::Microblock>,
        blks: &Range<i64>,
    ) -> Result<(), InsertionErrors> {
        Ok(self.microblocks.insert(&self.bq_client, req, blks).await?)
    }

    pub async fn get_txn_range(&self, start_at: i64) -> Result<Option<Range<i64>>> {
        Ok(self
            .transactions
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
    pub async fn is_txn_range_covered_by_entry(
        &self,
        start: i64,
        blks: i64,
    ) -> Result<Option<(i64, String)>> {
        self.transactions
            .is_range_covered_by_entry(&self.bq_client, start, blks)
            .await
    }

    async fn create_microblock_table(
        client: &Client,
        bq: &utils::BigQueryTableLocation,
    ) -> Result<Table> {
        let microblock_table = Table::new(
            &bq.dataset.project_id,
            &bq.dataset.dataset_id,
            &bq.table_id,
            TableSchema::new(vec![
                TableFieldSchema::integer("block"),
                TableFieldSchema::integer("offset_in_block"),
                TableFieldSchema::integer("shard_id"),
                TableFieldSchema::integer("header_version"),
                TableFieldSchema::bytes("header_committee_hash"),
                TableFieldSchema::bytes("header_prev_hash"),
                TableFieldSchema::integer("gas_limit"),
                TableFieldSchema::big_numeric("rewards"),
                TableFieldSchema::bytes("prev_hash"),
                TableFieldSchema::bytes("tx_root_hash"),
                TableFieldSchema::bytes("miner_pubkey"),
                TableFieldSchema::bytes("miner_addr_zil"),
                TableFieldSchema::bytes("miner_addr_eth"),
                TableFieldSchema::integer("ds_block_num"),
                TableFieldSchema::bytes("state_delta_hash"),
                TableFieldSchema::bytes("tran_receipt_hash"),
                TableFieldSchema::integer("block_shard_id"),
                TableFieldSchema::integer("gas_used"),
                TableFieldSchema::integer("epoch_num"),
                TableFieldSchema::integer("num_txs"),
                TableFieldSchema::bytes("blockhash"),
                TableFieldSchema::integer("timestamp"),
                TableFieldSchema::bytes("cs1"),
                TableFieldSchema::string("b1"),
                TableFieldSchema::bytes("cs2"),
                TableFieldSchema::string("b2"),
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
        let the_table = client.table().create(microblock_table).await;
        match the_table {
            Ok(tbl) => Ok(tbl),
            Err(_) => {
                // Wait a bit and then fetch the table.
                sleep(Duration::from_millis(5_000)).await;
                Ok(client
                    .table()
                    .get(
                        &bq.dataset.project_id,
                        &bq.dataset.dataset_id,
                        &bq.table_id,
                        Option::None,
                    )
                    .await?)
            }
        }
    }

    async fn create_transaction_table(
        client: &Client,
        bq: &utils::BigQueryTableLocation,
    ) -> Result<Table> {
        let transaction_table = Table::new(
            &bq.dataset.project_id,
            &bq.dataset.dataset_id,
            &bq.table_id,
            TableSchema::new(vec![
                TableFieldSchema::string("id"),
                TableFieldSchema::integer("block"),
                TableFieldSchema::integer("offset_in_block"),
                // Zilliqa version * 100
                TableFieldSchema::integer("zqversion"),
                TableFieldSchema::big_numeric("amount"),
                TableFieldSchema::string("api_type"),
                TableFieldSchema::bytes("code"),
                TableFieldSchema::bytes("data"),
                TableFieldSchema::integer("gas_limit"),
                TableFieldSchema::big_numeric("gas_price"),
                TableFieldSchema::integer("nonce"),
                TableFieldSchema::string("receipt"),
                TableFieldSchema::bytes("sender_public_key"),
                TableFieldSchema::string("from_addr_zil"),
                TableFieldSchema::string("from_addr_eth"),
                TableFieldSchema::bytes("signature"),
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
                    .get(
                        &bq.dataset.project_id,
                        &bq.dataset.dataset_id,
                        &bq.table_id,
                        Option::None,
                    )
                    .await?)
            }
        }
    }
}
