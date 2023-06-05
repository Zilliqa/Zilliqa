use crate::bq_object;
use anyhow::{anyhow, Result};
use gcp_bigquery_client::model::{
    dataset::Dataset, query_request::QueryRequest, query_response::ResultSet,
    range_partitioning::RangePartitioning, range_partitioning_range::RangePartitioningRange,
    table::Table, table_data_insert_all_request::TableDataInsertAllRequest,
    table_field_schema::TableFieldSchema, table_schema::TableSchema,
};
use gcp_bigquery_client::Client;
use serde::Serialize;
use std::fmt;
use std::marker::PhantomData;

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
    pub meta: Table,
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

pub const DATASET_ID: &str = "zilliqa";
pub const TRANSACTION_TABLE_ID: &str = "transactions";
pub const METADATA_TABLE_ID: &str = "meta";

impl ZilliqaBQProject {
    pub async fn new(project_id: &str, service_account_key_file: &str) -> Result<Self> {
        println!("A");
        // Application default creds don't work here because the auth library looks for a service
        // account key in the file you give it and, of course, it's not there ..
        let my_client = Client::from_service_account_key_file(service_account_key_file).await?;
        println!("B");
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

        let meta_table = if let Ok(tbl) = my_client
            .table()
            .get(project_id, DATASET_ID, METADATA_TABLE_ID, Option::None)
            .await
        {
            tbl
        } else {
            Self::create_meta_table(&my_client, project_id).await?
        };

        Ok(ZilliqaBQProject {
            project_id: project_id.to_string(),
            bq_client: my_client,
            ds: zq_ds,
            transactions: transaction_table,
            meta: meta_table,
        })
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
        max_blk: u64,
    ) -> Result<(), InsertionErrors> {
        let max_blk = i64::try_from(max_blk)
            .or_else(|e| Err(InsertionErrors::from_msg("Cannot convert max_blk to i64")))?;
        let mut err_rows = Vec::<String>::new();
        // Insert the transactions
        let resp = self
            .bq_client
            .tabledata()
            .insert_all(&self.project_id, DATASET_ID, TRANSACTION_TABLE_ID, req.req)
            .await
            .or_else(|e| Err(InsertionErrors::from_msg("Cannot insert")))?;
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
        // Insert the max row.
        // @TODO : Escape these values!
        let mut meta_result = self
            .bq_client
            .job()
            .query(
                &self.project_id,
                QueryRequest::new(format!(
                    "INSERT INTO {}.{}.{} (event_stamp, max_block) VALUES (CURRENT_DATETIME(), {})",
                    &self.project_id, DATASET_ID, METADATA_TABLE_ID, max_blk
                )),
            )
            .await
            .or_else(|e| Err(InsertionErrors::from_msg("Cannot update metadata")))?;
        Ok(())
    }

    /// Retrieve the maximum block known to have been imported.
    pub async fn get_max_block(&self) -> Result<u64> {
        let mut result = self
            .bq_client
            .job()
            .query(
                &self.project_id,
                QueryRequest::new(format!(
                    "SELECT MAX(max_block) FROM {}.{}.{}",
                    &self.project_id, DATASET_ID, METADATA_TABLE_ID
                )),
            )
            .await?;
        if result.row_count() < 1 || !result.next_row() {
            return Ok(0);
        }
        Ok(u64::try_from(result.get_i64(0)?.unwrap_or(0))?)
    }

    async fn create_meta_table(client: &Client, project_id: &str) -> Result<Table> {
        let metadata_table = Table::new(
            project_id,
            DATASET_ID,
            METADATA_TABLE_ID,
            TableSchema::new(vec![
                TableFieldSchema::date_time("event_stamp"),
                TableFieldSchema::integer("max_block"),
            ]),
        );
        Ok(client.table().create(metadata_table).await?)
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
