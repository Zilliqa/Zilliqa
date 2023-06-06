// Bigquery importer.
#[allow(unused_imports)]
use anyhow::{anyhow, Result};
use pdtbq::bq::ZilliqaBQProject;
use pdtbq::bq_object;
use pdtlib::exporter::Exporter;
use primitive_types::H256;

const PROJECT_ID: &str = "rrw-bigquery-test-id";
const SERVICE_ACCOUNT_KEY_FILE: &str =
    "/home/rrw/work/zilliqa/src/secrets/rrw-bigquery-test-id-8401353b2800.json";

pub async fn import(
    unpack_dir: &str,
    nr_machines: i64,
    machine_id: i64,
    batch_blks: i64,
) -> Result<()> {
    println!("Setting up schema");
    let client_id = format!("m{}_{}", machine_id, nr_machines);
    let exporter = Exporter::new(&unpack_dir)?;
    let max_block = exporter.get_max_block();
    let project = ZilliqaBQProject::new(
        PROJECT_ID,
        SERVICE_ACCOUNT_KEY_FILE,
        nr_machines,
        (max_block + 1).try_into()?,
        batch_blks,
        machine_id,
        &client_id,
    )
    .await?;

    println!("max_block is {}", max_block);
    println!("Requesting a block .. ");
    let maybe_range = project.get_range().await?;
    match maybe_range {
        None => {
            println!("Entire range already fetched; nothing to do");
            Ok(())
        }
        Some(range) => {
            // exporter.import_txns(4, max_block, None);
            println!("Retrieved block {:?}", range);
            let mut inserter = project.make_transaction_inserter().await?;
            let mut nr_txns = 0;
            for val in
                exporter.micro_blocks(1, range.end.try_into()?, Some(range.start.try_into()?))?
            {
                if let Ok((key, blk)) = val {
                    println!("Blk! at {}/{}", key.epochnum, key.shardid);
                    for (_hash, maybe_txn) in exporter.txns(&key, &blk)? {
                        if let Some(txn) = maybe_txn {
                            inserter.insert_row(&bq_object::Transaction::from_proto(
                                &txn.clone(),
                                <i64>::try_from(key.epochnum)?,
                                <i64>::try_from(key.shardid)?,
                            )?)?;
                            nr_txns += 1;
                            if let Some(actually_txn) = txn.transaction {
                                println!("Txn {}", H256::from_slice(&actually_txn.tranid));
                            }
                        }
                    }
                }
            }
            // Need to do this even if there are no transactions, to mark the range complete.
            println!("Inserting {} transactions for range {:?}", nr_txns, range);
            match project.insert_transactions(inserter, range).await {
                Err(val) => println!("Errors {:}", val),
                Ok(_) => (),
            }
            Ok(())
        }
    }
}
