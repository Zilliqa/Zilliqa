// Bigquery importer.
#[allow(unused_imports)]
use anyhow::{anyhow, Result};
use pdtbq::bq::ZilliqaBQProject;
use pdtbq::utils::{BigQueryDatasetLocation, ProcessCoordinates};
use pdtbq::values;
use pdtlib::exporter::Exporter;
use std::ops::Range;
use std::path::Path;
use tokio::task::JoinSet;

const PROJECT_ID: &str = "rrw-bigquery-test-id";
const DATASET_ID: &str = "testnet";
const SERVICE_ACCOUNT_KEY_FILE: &str =
    "/home/rrw/work/zilliqa/src/secrets/rrw-bigquery-test-id-8401353b2800.json";

pub async fn multi(
    unpack_dir: &str,
    nr_threads: i64,
    batch_blocks: i64,
    start_blk: Option<i64>,
) -> Result<()> {
    // OK. Just go ..
    let mut jobs = JoinSet::new();
    let location = BigQueryDatasetLocation {
        project_id: PROJECT_ID.to_string(),
        dataset_id: DATASET_ID.to_string(),
    };
    let coords = ProcessCoordinates {
        nr_machines: nr_threads,
        nr_blks: 0,
        batch_blks: batch_blocks,
        machine_id: 0,
        client_id: "schema_creator".to_string(),
    };

    // Start off by creating the schema. Allocating a new BQ Object will do ..
    {
        let _ = ZilliqaBQProject::new(&location, &coords, SERVICE_ACCOUNT_KEY_FILE).await?;
    }

    for idx in 0..nr_threads {
        let orig_unpack_dir = unpack_dir.to_string();
        let my_unpack_dir = Path::new(unpack_dir).join("..").join(format!("t{}", idx));
        let my_start_blk = start_blk.clone();
        jobs.spawn(async move {
            println!("Sync persistence to {:?}", &my_unpack_dir);
            let unpack_str = my_unpack_dir
                .to_str()
                .ok_or(anyhow!("Cannot render thread-specific data dir"))?;
            // Sync persistence
            pdtlib::utils::dup_directory(&orig_unpack_dir, &unpack_str)?;
            println!("Starting thread {}/{}", idx, nr_threads);
            import(
                &unpack_str,
                nr_threads,
                idx,
                batch_blocks,
                None,
                my_start_blk,
            )
            .await
        });
    }

    println!("Waiting for jobs to complete .. ");
    while jobs.len() > 0 {
        let result = jobs.join_next().await;
        if let Some(res) = result {
            let value = res?;
            println!("Job finished with {:?}, {} remain", value, jobs.len())
        }
    }
    Ok(())
}

pub async fn import(
    unpack_dir: &str,
    nr_machines: i64,
    machine_id: i64,
    batch_blks: i64,
    nr_batches: Option<i64>,
    start_block: Option<i64>,
) -> Result<()> {
    println!("Setting up schema");
    let client_id = format!("m{}_{}", machine_id, nr_machines);
    let exporter = Exporter::new(&unpack_dir)?;
    let max_block = exporter.get_max_block();
    let location = BigQueryDatasetLocation {
        project_id: PROJECT_ID.to_string(),
        dataset_id: DATASET_ID.to_string(),
    };
    let coords = ProcessCoordinates {
        nr_machines,
        nr_blks: (max_block + 1).try_into()?,
        batch_blks: batch_blks,
        machine_id,
        client_id: client_id.to_string(),
    };

    let project = ZilliqaBQProject::new(&location, &coords, SERVICE_ACCOUNT_KEY_FILE).await?;

    println!("max_block is {}", max_block);
    let mut batch = 0;
    let mut last_max = match start_block {
        Some(x) => x,
        None => 0,
    };
    while match nr_batches {
        None => true,
        Some(val) => batch < val,
    } {
        println!("Requesting a block .. ");
        let maybe_range = project.get_txn_range(last_max).await?;
        match maybe_range {
            None => {
                println!("Entire range already fetched; nothing to do");
                return Ok(());
            }
            Some(range) => {
                println!("{}: Retrieved block {:?}", client_id, range);
                let mut inserter = project.make_inserter::<values::Transaction>().await?;
                let mut mb_inserter = project.make_inserter::<values::Microblock>().await?;
                let mut nr_txns = 0;
                for val in
                    exporter.micro_blocks(range.end.try_into()?, Some(range.start.try_into()?))?
                {
                    if let Ok((key, blk)) = val {
                        let mut offset_in_block = 0;
                        mb_inserter.insert_row(values::Microblock::from_proto(
                            &blk.clone(),
                            <i64>::try_from(key.epochnum)?,
                            <i64>::try_from(key.shardid)?,
                        )?)?;

                        // println!("Blk! at {}/{}", key.epochnum, key.shardid);
                        for (_hash, maybe_txn) in exporter.txns(&key, &blk)? {
                            // println!("hash {}", _hash);
                            if let Some(txn) = maybe_txn {
                                inserter.insert_row(values::Transaction::from_proto(
                                    &txn.clone(),
                                    <i64>::try_from(key.epochnum)?,
                                    offset_in_block,
                                    <i64>::try_from(key.shardid)?,
                                )?)?;
                                nr_txns += 1;
                                offset_in_block += 1;
                                if let Some(_actually_txn) = txn.transaction {
                                    // println!("Txn {}", H256::from_slice(&actually_txn.tranid));
                                }
                            }
                        }
                    }
                }
                // Need to do this even if there are no transactions, to mark the range complete.
                println!(
                    "{}: Inserting {} transactions for range {:?}",
                    client_id, nr_txns, range
                );

                match project.insert_microblocks(mb_inserter, &range).await {
                    Err(val) => {
                        println!("{}: Cannot import microblocks! {:}", client_id, val);
                    }
                    Ok(_) => (),
                }
                match project.insert_transactions(inserter, &range).await {
                    Err(val) => println!("{}: Errors {:}", client_id, val),
                    Ok(_) => (),
                };

                // The last max is where we stopped, not where the range ended - we may
                // have multiple tranches to fetch in the range returned.
                // TODO: Do these manually, rather than going back to BQ every time.
                last_max = range.end;
            }
        }
        batch += 1;
    }
    Ok(())
}

pub async fn reconcile_blocks(unpack_dir: &str, scale: i64) -> Result<()> {
    let client_id = format!("reconcile-blocks");
    let exporter = Exporter::new(&unpack_dir)?;
    let max_block: i64 = exporter.get_max_block().try_into()?;
    let location = BigQueryDatasetLocation {
        project_id: PROJECT_ID.to_string(),
        dataset_id: DATASET_ID.to_string(),
    };
    let coords = ProcessCoordinates {
        nr_machines: 1,
        nr_blks: max_block + 1,
        batch_blks: 0,
        machine_id: 0,
        client_id: client_id.to_string(),
    };
    let project = ZilliqaBQProject::new(&location, &coords, SERVICE_ACCOUNT_KEY_FILE).await?;
    // We should have coverage for every block, extant or not, up to the
    // last batch, which will be short.
    let mut blk: i64 = 0;
    while blk < max_block {
        let span = std::cmp::min(scale, max_block - (blk + scale));
        println!("blk {} span {}", blk, span);
        match project.is_txn_range_covered_by_entry(blk, span).await? {
            None => {
                println!(
                    "Help! Block range {} + {} is not covered by any imported entry",
                    blk, span
                );
                blk += span;
            }
            Some((nr_blks, _c)) => {
                println!("Skipping {} blocks @ {}", nr_blks, blk);
                blk += nr_blks;
            }
        };
    }

    Ok(())
}
