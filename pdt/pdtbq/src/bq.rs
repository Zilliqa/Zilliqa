use crate::bq_object;
use crate::meta::Meta;
#[allow(unused_imports)]
use anyhow::{anyhow, Result};
use gcp_bigquery_client::model::{
    dataset::Dataset, range_partitioning::RangePartitioning,
    range_partitioning_range::RangePartitioningRange, table::Table,
    table_data_insert_all_request::TableDataInsertAllRequest, table_field_schema::TableFieldSchema,
    table_schema::TableSchema,
};
use gcp_bigquery_client::Client;
use serde::Serialize;
use std::fmt;
use std::marker::PhantomData;
use std::ops::Range;

pub const DATASET_ID: &str = "zilliqa";
pub const TRANSACTION_TABLE_ID: &str = "transactions";
pub const METADATA_TABLE_ID: &str = "meta";

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
}

pub struct Inserter<T: Serialize> {
    _marker: PhantomData<T>,
    req: TableDataInsertAllRequest,
}

impl<T: Serialize> Inserter<T> {
    pub fn insert_row(&mut self, row: &T) -> Result<()> {
        Ok(self.req.add_row(None, &row)?)
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

        Ok(ZilliqaBQProject {
            project_id: project_id.to_string(),
            bq_client: my_client,
            ds: zq_ds,
            transactions: transaction_table,
            meta,
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
            req: TableDataInsertAllRequest::new(),
        })
    }

    /// Act on an inserter.
    pub async fn insert_transactions(
        &self,
        req: Inserter<bq_object::Transaction>,
        blks: &Range<i64>,
    ) -> Result<(), InsertionErrors> {
        let mut err_rows = Vec::<String>::new();
        if req.req.len() > 0 {
            // Insert the transactions
            let resp = self
                .bq_client
                .tabledata()
                .insert_all(&self.project_id, DATASET_ID, TRANSACTION_TABLE_ID, req.req)
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

    pub async fn get_range(&self, start_at: i64) -> Result<Option<Range<i64>>> {
        Ok(self
            .meta
            .find_next_range_to_do(&self.bq_client, start_at)
            .await?)
    }

    /// If a single entry in the meta table contains start  .. start+blks , then return the client id
    /// that generated it, else return None.
    pub async fn is_range_covered_by_entry(&self, start: i64, blks: i64) -> Result<Option<String>> {
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
                TableFieldSchema::big_numeric("amount"),
                TableFieldSchema::string("code"),
                TableFieldSchema::string("data"),
                TableFieldSchema::integer("gas_limit"),
                TableFieldSchema::big_numeric("gas_price"),
                TableFieldSchema::integer("nonce"),
                // This will eventually move to the receipts table.
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
        Ok(client.table().create(transaction_table).await?)
    }
}
