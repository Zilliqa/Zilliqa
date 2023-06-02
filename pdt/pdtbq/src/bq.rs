use eyre::Result;
use gcp_bigquery_client::model::{
    dataset::Dataset, range_partitioning::RangePartitioning,
    range_partitioning_range::RangePartitioningRange, table::Table,
    table_field_schema::TableFieldSchema, table_schema::TableSchema,
};
use gcp_bigquery_client::Client;

pub struct ZilliqaBQProject {
    pub project_id: String,
    pub bq_client: Client,
    pub ds: Dataset,
    pub transactions: Table,
}

pub const DATASET_ID: &str = "zilliqa";
pub const TRANSACTION_TABLE_ID: &str = "transactions";

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

        Ok(ZilliqaBQProject {
            project_id: project_id.to_string(),
            bq_client: my_client,
            ds: zq_ds,
            transactions: transaction_table,
        })
    }

    pub async fn create_transaction_table(client: &Client, project_id: &str) -> Result<Table> {
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
                TableFieldSchema::integer("gas_price"),
                TableFieldSchema::integer("nonce"),
                // This will eventually move to the receipts table.
                TableFieldSchema::string("receipt"),
                TableFieldSchema::string("sender_public_key"),
                TableFieldSchema::string("signature"),
                TableFieldSchema::string("to_addr"),
                TableFieldSchema::integer("version"),
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
