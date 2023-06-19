use crate::utils;
#[allow(unused_imports)]
use anyhow::{anyhow, Result};
use gcp_bigquery_client::model::{
    query_request::QueryRequest, table::Table, table_field_schema::TableFieldSchema,
    table_schema::TableSchema,
};
use gcp_bigquery_client::Client;
use std::ops::Range;
use tokio::time::{sleep, Duration};

pub struct Meta {
    table: utils::BigQueryTableLocation,
    pub coords: utils::ProcessCoordinates,
}

/// A metadata table holds a list of the ranges which have been imported,
/// so that we can arrange to import the rest.
/// it contains records:
/// (start_block, end_block, when)
/// Given an end block, we:
///  * Divide our blocks into groups of batch_blks
///  * We'll look at (blk/batch_blks)%m == %n
///  * Find the first of our blocks not done.
///  * Work on it.
///
/// This is somewhat challenging in an append-only database.
/// There are several ways to solve this problem, but all of them are kinda
/// hard to implement.
/// The way we do it is to store two kinds of records:
///   -> min=0; this means everything is done up to max
///   -> min != 0; this means there is a gap at the bottom.
///
impl Meta {
    /// Create a structure
    pub fn new(
        bq: &utils::BigQueryTableLocation,
        coords: &utils::ProcessCoordinates,
    ) -> Result<Self> {
        Ok(Meta {
            table: bq.clone(),
            coords: coords.clone(),
        })
    }

    pub async fn ensure_table(&self, client: &Client) -> Result<()> {
        if let Err(_) = client
            .table()
            .get(
                &self.table.dataset.project_id,
                &self.table.dataset.dataset_id,
                &self.table.table_id,
                Option::None,
            )
            .await
        {
            self.create_table(client).await?;
        }
        Ok(())
    }

    pub async fn create_table(&self, client: &Client) -> Result<Table> {
        let metadata_table = Table::new(
            &self.table.dataset.project_id,
            &self.table.dataset.dataset_id,
            &self.table.table_id,
            TableSchema::new(vec![
                TableFieldSchema::string("client_id"),
                TableFieldSchema::date_time("event_stamp"),
                TableFieldSchema::integer("start_blk"),
                TableFieldSchema::integer("nr_blks"),
            ]),
        );
        let the_table = client.table().create(metadata_table).await;
        match the_table {
            Ok(tbl) => Ok(tbl),
            Err(_) => {
                // Wait a bit and then fetch the table.
                sleep(Duration::from_millis(5_000)).await;
                Ok(client
                    .table()
                    .get(
                        &self.table.dataset.project_id,
                        &self.table.dataset.dataset_id,
                        &self.table.table_id,
                        Option::None,
                    )
                    .await?)
            }
        }
    }

    /// decide if the range start..nr_blks is covered by the metadata table already.
    pub async fn is_range_covered_by_entry(
        &self,
        client: &Client,
        start: i64,
        nr_blks: i64,
    ) -> Result<Option<(i64, String)>> {
        let query = format!("SELECT start_blk,nr_blks,client_id FROM {} WHERE start_blk <= {} and nr_blks >= {} - start_blk ORDER BY nr_blks DESC LIMIT 1",
                            self.table.get_table_desc(), start, nr_blks + start);
        let mut result = client
            .job()
            .query(&self.table.dataset.project_id, QueryRequest::new(&query))
            .await?;
        if result.next_row() {
            Ok(Some((
                result.get_i64(1)?.ok_or(anyhow!("No nr_blks in record"))?,
                result
                    .get_string(2)?
                    .ok_or(anyhow!("No client id in record"))?,
            )))
        } else {
            Ok(None)
        }
    }

    /// Find the next range of blocks for this client to perform, starting at start_at_in.
    pub async fn find_next_range_to_do(
        &self,
        client: &Client,
        start_at_in: i64,
    ) -> Result<Option<Range<i64>>> {
        let mut start_at: i64 = start_at_in;
        loop {
            let next_range = self
                .find_next_free_range(client, start_at, self.coords.nr_blks)
                .await?;

            println!(
                "{}: next_range {:?} start_at {} max_blk {}",
                self.coords.client_id, next_range, start_at, self.coords.nr_blks
            );
            // The next range starts above the max_blk, so we don't really care.
            if next_range.start >= self.coords.nr_blks {
                return Ok(None);
            }

            // OK. Does this range overlap one of my batches? The batch starts at (next_range.start/batch_blk*nr_machines)
            // Our next batch is at start + nr * batch_size.

            // skip_blocks is the gap between one of our batches and the next one.
            let skip_blocks = self.coords.batch_blks * self.coords.nr_machines;
            let batch_start = next_range.start - next_range.start % (skip_blocks);
            let mut our_next_batch_start =
                batch_start + (self.coords.machine_id * self.coords.batch_blks);
            let mut our_next_batch_end = our_next_batch_start + self.coords.batch_blks;
            println!(
                "batch_start {} our_next_batch_start {} our_next_batch_end {}",
                batch_start, our_next_batch_start, our_next_batch_end
            );
            // Make sure we address a block range above the start of the range of unprocessed blocks
            // we've found. Compare the end of the batch, because otherwise if we end up in the middle
            // of one of our batches, we'll never process the end of it.
            while our_next_batch_end <= next_range.start {
                our_next_batch_start += skip_blocks;
                our_next_batch_end += skip_blocks;
            }

            // Does it overlap?
            println!(
                "{}: ours {} .. {}",
                self.coords.client_id, our_next_batch_start, our_next_batch_end
            );
            let start_range = std::cmp::max(next_range.start, our_next_batch_start);
            let end_range = std::cmp::min(next_range.end, our_next_batch_end);
            if start_range < next_range.end && end_range > next_range.start {
                let result = Range {
                    start: start_range,
                    end: end_range,
                };
                println!("{}: OK. Fetching {:?}", self.coords.client_id, result);
                return Ok(Some(result));
            }
            start_at = next_range.end;
        }
    }

    /// Find the next set of done blocks above this one and return the range between them
    pub async fn find_next_gap_above(
        &self,
        client: &Client,
        blk_to_find: i64,
    ) -> Result<Option<Range<i64>>> {
        let mut result = client
            .job()
            .query(&self.table.dataset.project_id,
                   QueryRequest::new(format!("SELECT start_blk, nr_blks FROM {} WHERE start_blk >= {} AND nr_blks > 0  ORDER BY start_blk ASC, nr_blks DESC LIMIT 1",
                                             self.table.get_table_desc(), blk_to_find))).await?;
        if result.next_row() {
            let start_blk = result
                .get_i64(0)?
                .ok_or(anyhow!("No start value in range read"))?;
            let nr_blks = result
                .get_i64(1)?
                .ok_or(anyhow!("No end value in range read"))?;
            Ok(Some(Range {
                start: start_blk,
                end: start_blk + nr_blks,
            }))
        } else {
            Ok(None)
        }
    }

    pub fn get_nr_blocks(&self) -> i64 {
        return self.coords.nr_blks;
    }
    pub async fn commit_run(&self, client: &Client, range: &Range<i64>) -> Result<()> {
        let _ = client
            .job()
            .query(&self.table.dataset.project_id,
                   QueryRequest::new(format!("INSERT INTO {} (client_id, event_stamp, start_blk, nr_blks) VALUES (\"{}\",CURRENT_DATETIME(), {}, {})",
                                             self.table.get_table_desc(),
                                             self.coords.client_id, range.start, range.end-range.start)))
            .await?;
        Ok(())
    }

    pub async fn find_next_free_range(
        &self,
        client: &Client,
        start_at: i64,
        max_blk: i64,
    ) -> Result<Range<i64>> {
        let mut current_run: Option<Range<i64>> = None;
        let mut should_commit_current_run = false;
        let mut result: Option<Range<i64>> = None;
        // Otherwise, we're set up.
        println!(
            "{}, find_next_free_range() start at {} max {}",
            self.coords.client_id, start_at, max_blk
        );
        while result.is_none() {
            let blk_to_find = match &current_run {
                Some(val) => val.end,
                None => start_at,
            };
            println!(
                "{}: Find next range above {}",
                self.coords.client_id, blk_to_find
            );
            match self.find_next_gap_above(client, blk_to_find).await? {
                None => {
                    // There is no next range. Commit the current run if we should
                    println!("{}: No next range", self.coords.client_id);
                    result = Some(Range {
                        start: blk_to_find,
                        end: max_blk + 1,
                    });
                }
                Some(have_run) => {
                    // There is a next range of done blocks. Is it contiguous with the one we have?
                    println!(
                        "{}: Next range {:?} current {:?}",
                        self.coords.client_id, have_run, current_run
                    );
                    if let Some(current_run_val) = &current_run {
                        if current_run_val.end == have_run.start {
                            // Merge them
                            current_run = Some(Range {
                                start: current_run_val.start,
                                end: have_run.end,
                            });
                            should_commit_current_run = true;
                            println!(
                                "{}: Extended run to {:?}",
                                self.coords.client_id, current_run
                            );
                        } else {
                            result = Some(Range {
                                start: current_run_val.end,
                                end: have_run.start,
                            });
                            println!("{}: Found a gap at {:?}", self.coords.client_id, result);
                        }
                    } else {
                        // There was no current run. make one.
                        if have_run.start > start_at {
                            // There's a hole at the beginning
                            result = Some(Range {
                                start: start_at,
                                end: have_run.start,
                            });
                            println!(
                                "{}: Hole at the start result = {:?}",
                                self.coords.client_id, result
                            );
                        }
                        current_run = Some(have_run);
                        should_commit_current_run = false;
                    }
                }
            }
        }
        if let Some(run_val) = current_run {
            if should_commit_current_run {
                println!("{}: Commit run {:?}", self.coords.client_id, run_val);
                self.commit_run(client, &run_val).await?;
            }
        }
        // Legal because if the result is not present here, something has gone very wrong
        // with our logic.
        println!("{}: Result {:?}", self.coords.client_id, result);
        Ok(result.unwrap())
    }
}
