// Bigquery importer.
#[allow(unused_imports)]
use anyhow::{anyhow, Result};
use async_trait::async_trait;
use pdtbq::bq::{Inserter, ZilliqaBQProject};
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

#[async_trait]
pub trait Importer {
    /// Retrieve an id so we know what importer we're talking about.
    fn get_id(&self) -> String;

    /// Set the client id
    fn set_client_id(&mut self, client_id: &str);

    /// Retrieve the max block for this round of import
    async fn get_max_block(&self, exp: &Exporter) -> Result<i64>;

    /// Get the next range for this import
    async fn maybe_range(
        &self,
        project: &ZilliqaBQProject,
        last_max: i64,
    ) -> Result<Option<Range<i64>>>;

    /// Set up an internal buffer.
    async fn extract_start(
        &mut self,
        project: &ZilliqaBQProject,
        exporter: &Exporter,
    ) -> Result<()>;

    /// Extract a range from the database to an internal buffer.
    async fn extract_range(
        &mut self,
        project: &ZilliqaBQProject,
        exporter: &Exporter,
        range: &Range<i64>,
    ) -> Result<()>;

    /// Insert the internal buffer into the database
    async fn extract_done(&mut self, project: &ZilliqaBQProject, exporter: &Exporter)
        -> Result<()>;
}

/// We import transactions and microblocks at the same time because they use the same db.
pub struct TransactionMicroblockImporter {
    txn_inserter: Option<Inserter<values::Transaction>>,
    mb_inserter: Option<Inserter<values::Microblock>>,
    range: Option<Range<i64>>,
    client_id: String,
}

impl TransactionMicroblockImporter {
    pub fn new() -> Self {
        Self {
            txn_inserter: None,
            mb_inserter: None,
            range: None,
            client_id: "unset".to_string(),
        }
    }
}

#[async_trait]
impl Importer for TransactionMicroblockImporter {
    fn get_id(&self) -> String {
        "m".to_string()
    }

    fn set_client_id(&mut self, client_id: &str) {
        self.client_id = client_id.to_string();
    }

    async fn get_max_block(&self, exp: &Exporter) -> Result<i64> {
        Ok(exp.get_max_block().try_into()?)
    }

    async fn maybe_range(
        &self,
        project: &ZilliqaBQProject,
        last_max: i64,
    ) -> Result<Option<Range<i64>>> {
        Ok(project.get_txn_range(last_max).await?)
    }

    /// Set up an internal buffer.
    async fn extract_start(
        &mut self,
        project: &ZilliqaBQProject,
        _exporter: &Exporter,
    ) -> Result<()> {
        self.txn_inserter = Some(project.make_inserter::<values::Transaction>().await?);
        self.mb_inserter = Some(project.make_inserter::<values::Microblock>().await?);
        self.range = None;
        Ok(())
    }

    /// Extract a range from the database to an internal buffer.
    async fn extract_range(
        &mut self,
        _project: &ZilliqaBQProject,
        exporter: &Exporter,
        range: &Range<i64>,
    ) -> Result<()> {
        if let Some(_) = self.range {
            return Err(anyhow!(
                "Attempt to call extract_range when range is {:?} and not None",
                range
            ));
        }
        let mut nr_txns = 0;
        for val in exporter.micro_blocks(range.end.try_into()?, Some(range.start.try_into()?))? {
            if let Ok((key, blk)) = val {
                let mut offset_in_block = 0;
                if let Some(mb_inserter) = self.mb_inserter.as_mut() {
                    mb_inserter.insert_row(values::Microblock::from_proto(
                        &blk.clone(),
                        <i64>::try_from(key.epochnum)?,
                        <i64>::try_from(key.shardid)?,
                    )?)?;
                }
                // println!("Blk! at {}/{}", key.epochnum, key.shardid);
                for (_hash, maybe_txn) in exporter.txns(&key, &blk)? {
                    // println!("hash {}", _hash);
                    if let Some(txn) = maybe_txn {
                        if let Some(inserter) = self.txn_inserter.as_mut() {
                            inserter.insert_row(values::Transaction::from_proto(
                                &txn.clone(),
                                <i64>::try_from(key.epochnum)?,
                                offset_in_block,
                                <i64>::try_from(key.shardid)?,
                            )?)?;
                        }
                        nr_txns += 1;
                        offset_in_block += 1;
                        if let Some(_actually_txn) = txn.transaction {
                            // println!("Txn {}", H256::from_slice(&actually_txn.tranid));
                        }
                    }
                }
            }
        }
        self.range = Some(range.clone());
        println!("{}: accumulated {} txns", self.client_id, nr_txns);
        Ok(())
    }

    /// Insert the internal buffer into the database
    async fn extract_done(
        &mut self,
        project: &ZilliqaBQProject,
        _exporter: &Exporter,
    ) -> Result<()> {
        if let Some(range) = &self.range {
            if let Some(mb_inserter) = self.mb_inserter.take() {
                match project.insert_microblocks(mb_inserter, range).await {
                    Err(val) => {
                        println!("{}: Cannot import microblocks! {:}", self.client_id, val);
                    }
                    Ok(_) => (),
                }
            }
            if let Some(txn_inserter) = self.txn_inserter.take() {
                match project.insert_transactions(txn_inserter, range).await {
                    Err(val) => println!("{}: Errors {:}", self.client_id, val),
                    Ok(_) => (),
                };
            }
        } else {
            println!("{}: No data to export to database", self.client_id);
        }
        Ok(())
    }
}

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
        batch_blks: batch_blocks,
        machine_id: 0,
        client_id: "schema_creator".to_string(),
    };

    // Start off by creating the schema. Allocating a new BQ Object will do ..
    println!("Creating schema .. ");
    ZilliqaBQProject::ensure_schema(&location, SERVICE_ACCOUNT_KEY_FILE).await?;

    for idx in 0..nr_threads {
        let orig_unpack_dir = unpack_dir.to_string();
        let my_unpack_dir = Path::new(unpack_dir).join("..").join(format!("t{}", idx));
        let my_start_blk = start_blk.clone();
        let my_coords = coords
            .with_client_id(&format!("t{}_{}", idx, coords.nr_machines))
            .with_machine_id(idx);
        let location = BigQueryDatasetLocation {
            project_id: PROJECT_ID.to_string(),
            dataset_id: DATASET_ID.to_string(),
        };

        jobs.spawn(async move {
            println!("Sync persistence to {:?}", &my_unpack_dir);
            let unpack_str = my_unpack_dir
                .to_str()
                .ok_or(anyhow!("Cannot render thread-specific data dir"))?;
            // Sync persistence
            pdtlib::utils::dup_directory(&orig_unpack_dir, &unpack_str)?;
            println!("Starting thread {}/{}", idx, nr_threads);
            let exporter = Exporter::new(&unpack_str)?;
            let mut imp = TransactionMicroblockImporter::new();
            let max_block = imp.get_max_block(&exporter).await?;
            let nr_blocks: i64 = (max_block + 1).try_into()?;
            let project =
                ZilliqaBQProject::new(&location, &my_coords, SERVICE_ACCOUNT_KEY_FILE, nr_blocks)
                    .await?;
            while !import(
                &mut imp,
                &exporter,
                &project,
                &my_coords,
                None,
                my_start_blk,
            )
            .await?
            {
                // Go round again!
            }
            let result: anyhow::Result<()> = Ok(());
            result
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

/// Returns true if we're done, false if we're not.
pub async fn import<T: Importer>(
    imp: &mut T,
    exporter: &Exporter,
    project: &ZilliqaBQProject,
    in_coords: &ProcessCoordinates,
    nr_batches: Option<i64>,
    start_block: Option<i64>,
) -> Result<bool> {
    let client_id = format!(
        "{}{}_{}",
        imp.get_id(),
        in_coords.machine_id,
        in_coords.nr_machines
    );
    imp.set_client_id(&client_id);

    let coords = in_coords.with_client_id(&client_id);

    let mut batch = 0;
    let mut last_max = match start_block {
        Some(x) => x,
        None => 0,
    };
    while match nr_batches {
        None => true,
        Some(val) => batch < val,
    } {
        println!("{}: requesting a block .. ", coords.client_id);
        let maybe_range = imp.maybe_range(project, last_max).await?;
        match maybe_range {
            None => {
                println!("{}: range fetched. All done.", coords.client_id);
                return Ok(true);
            }
            Some(range) => {
                // Curses! Work to do..
                println!("{}: work to do at {:?}", coords.client_id, range);
                imp.extract_start(project, exporter).await?;
                imp.extract_range(project, exporter, &range).await?;
                println!("{}: inserting records.. ", coords.client_id);
                imp.extract_done(project, exporter).await?;
                last_max = range.end;
            }
        }
        batch += 1;
    }
    Ok(false)
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
        batch_blks: 0,
        machine_id: 0,
        client_id: client_id.to_string(),
    };
    let project =
        ZilliqaBQProject::new(&location, &coords, SERVICE_ACCOUNT_KEY_FILE, max_block + 1).await?;
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
