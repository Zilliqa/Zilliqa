// Bigquery importer.
#[allow(unused_imports)]
use anyhow::{anyhow, Result};
use pdtbq::bq::ZilliqaBQProject;
use pdtbq::bq_object;
use pdtlib::exporter::Exporter;
use std::ops::Range;
use std::path::Path;
use tokio::task::JoinSet;

const PROJECT_ID: &str = "rrw-bigquery-test-id";
const SERVICE_ACCOUNT_KEY_FILE: &str =
    "/home/rrw/work/zilliqa/src/secrets/rrw-bigquery-test-id-8401353b2800.json";

pub async fn multi(unpack_dir: &str, nr_threads: i64, batch_blocks: i64) -> Result<()> {
    // OK. Just go ..
    let mut jobs = JoinSet::new();

    for idx in 0..nr_threads {
        let orig_unpack_dir = unpack_dir.to_string();
        let my_unpack_dir = Path::new(unpack_dir).join("..").join(format!("t{}", idx));
        jobs.spawn(async move {
            println!("Sync persistence to {:?}", &my_unpack_dir);
            let unpack_str = my_unpack_dir
                .to_str()
                .ok_or(anyhow!("Cannot render thread-specific data dir"))?;
            // Sync persistence
            pdtlib::utils::dup_directory(&orig_unpack_dir, &unpack_str)?;
            println!("Starting thread {}/{}", idx, nr_threads);
            import(&unpack_str, nr_threads, idx, batch_blocks, None).await
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
    let mut batch = 0;
    let mut last_max = 0;
    while match nr_batches {
        None => true,
        Some(val) => batch < val,
    } {
        println!("Requesting a block .. ");
        let maybe_range = project.get_range(last_max).await?;
        match maybe_range {
            None => {
                println!("Entire range already fetched; nothing to do");
                return Ok(());
            }
            Some(range) => {
                // exporter.import_txns(4, max_block, None);
                println!("{}: Retrieved block {:?}", client_id, range);
                let mut inserter = project.make_transaction_inserter().await?;
                let mut nr_txns = 0;
                for val in exporter.micro_blocks(
                    1,
                    range.end.try_into()?,
                    Some(range.start.try_into()?),
                )? {
                    if let Ok((key, blk)) = val {
                        // println!("Blk! at {}/{}", key.epochnum, key.shardid);
                        for (_hash, maybe_txn) in exporter.txns(&key, &blk)? {
                            if let Some(txn) = maybe_txn {
                                inserter.insert_row(&bq_object::Transaction::from_proto(
                                    &txn.clone(),
                                    <i64>::try_from(key.epochnum)?,
                                    <i64>::try_from(key.shardid)?,
                                )?)?;
                                nr_txns += 1;
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
                match project.insert_transactions(inserter, &range).await {
                    Err(val) => println!("{}: Errors {:}", client_id, val),
                    Ok(_) => (),
                }
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
    let max_block = exporter.get_max_block();
    let project = ZilliqaBQProject::new(
        PROJECT_ID,
        SERVICE_ACCOUNT_KEY_FILE,
        1,
        (max_block + 1).try_into()?,
        0,
        0,
        &client_id,
    )
    .await?;
    // For each contiguous range, check that it has been imported.
    let mut ranges: Vec<Range<i64>> = Vec::new();
    {
        let mut current_range: Option<Range<i64>> = None;

        println!("Deriving available block ranges .. ");
        let mut progress_counter = 0;
        for val in exporter.micro_blocks(1, max_block, None)? {
            if let Ok((key, _)) = val {
                let blk_num: i64 = key.epochnum.try_into()?;
                if progress_counter % 10000 == 0 {
                    println!("..{} ..", blk_num)
                }
                progress_counter += 1;
                current_range = match current_range {
                    None => Some(Range {
                        start: blk_num,
                        end: blk_num + 1,
                    }),
                    Some(old_range) => {
                        if blk_num == old_range.end {
                            Some(Range {
                                start: old_range.start,
                                end: blk_num + 1,
                            })
                        } else {
                            // There is a missing block.
                            ranges.push(old_range.clone());
                            Some(Range {
                                start: blk_num,
                                end: blk_num + 1,
                            })
                        }
                    }
                }
            }
        }
        if let Some(r) = current_range {
            ranges.push(r);
        }
    }

    println!("Ranges: {:?}", ranges);
    // Refine ranges to 10k blocks. They are already sorted because of the way we
    // generated them.
    let mut ranges_refined: Vec<Range<i64>> = Vec::new();
    {
        let mut current_range: Option<Range<i64>> = None;
        for r in ranges {
            let rounded = Range {
                start: r.start / scale,
                end: ((r.end + scale - 1) / scale) + 1,
            };
            current_range = match current_range {
                None => Some(rounded),
                Some(running) => {
                    if rounded.end <= running.end {
                        Some(Range {
                            start: running.start,
                            end: running.end,
                        })
                    } else if rounded.start == running.end {
                        Some(Range {
                            start: running.start,
                            end: rounded.end,
                        })
                    } else {
                        // Gap!
                        ranges_refined.push(running.clone());
                        Some(rounded)
                    }
                }
            }
        }
        if let Some(running) = current_range {
            ranges_refined.push(running);
        }
    }
    println!("Ranges refined: {:?}", ranges_refined);
    println!("There are {} ranges", ranges_refined.len());
    // Now check each refined range. Annoyingly, we have to do this one by one, so we can be fairly sure
    // our amalgamation alg hasn't screwed up..
    for r in ranges_refined {
        for blk_range in r.start..r.end {
            // println!("Check block range {}", blk_range);
            match project
                .is_range_covered_by_entry(blk_range * scale, scale)
                .await?
            {
                None => println!(
                    "Help! Block range {} + {} is not covered by any imported entry",
                    blk_range * scale,
                    scale
                ),
                Some(_c) => (), // println!("{}", c)},
            }
        }
    }

    Ok(())
}
