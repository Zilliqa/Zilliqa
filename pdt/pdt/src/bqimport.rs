// Bigquery importer.
use anyhow::{anyhow, Result};
use pdtbq::bq::ZilliqaBQProject;
use pdtbq::bq_object;
use pdtlib::exporter::Exporter;
use primitive_types::H256;

const PROJECT_ID: &str = "rrw-bigquery-test-id";
const SERVICE_ACCOUNT_KEY_FILE: &str =
    "/home/rrw/work/zilliqa/src/secrets/rrw-bigquery-test-id-8401353b2800.json";

pub async fn import(unpack_dir: &str) -> Result<()> {
    println!("Setting up schema");
    let project = ZilliqaBQProject::new(PROJECT_ID, SERVICE_ACCOUNT_KEY_FILE).await?;

    let exporter = Exporter::new(&unpack_dir)?;
    let max_block = exporter.get_max_block();
    println!("max_block is {}", max_block);
    let max_block_imported = project.get_max_block().await?;
    println!("max block already imported is {}", max_block_imported);

    // exporter.import_txns(4, max_block, None);
    for val in exporter.micro_blocks(1, max_block, Some(max_block_imported))? {
        if let Ok((key, blk)) = val {
            let mut inserter = project.make_transaction_inserter().await?;
            let mut nr_txns = 0;
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
            if nr_txns > 0 {
                println!(
                    "Inserting {} transactions for block {}",
                    nr_txns, key.epochnum
                );
                match project.insert_transactions(inserter, key.epochnum).await {
                    Err(val) => println!("Errors {:}", val),
                    Ok(_) => (),
                }
            }
        } else {
            println!("Error!");
        }
    }

    Ok(())
}
