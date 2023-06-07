mod bqimport;

use anyhow::Result;
use clap::{Args, Parser, Subcommand};
use pdtlib::context::Context;
use pdtlib::exporter::Exporter;
use pdtlib::historical::Historical;
use pdtlib::incremental::Incremental;
use pdtlib::render::Renderer;
use primitive_types::H256;

#[derive(Parser)]
#[command(author, version, about, long_about = None)]
struct Cli {
    #[arg(long, default_value = "/home/rrw/tmp/test")]
    download_dir: String,

    #[arg(long, default_value = "/home/rrw/tmp/unpacked")]
    unpack_dir: String,

    #[command(subcommand)]
    cmd: Commands,
}

#[derive(Subcommand)]
enum Commands {
    #[command(name = "download")]
    DownloadPersistence,
    #[command(name = "dump")]
    DumpPersistence,
    #[command(name = "bq")]
    ImportBq(ImportOptions),
    #[command(name = "bqmulti")]
    ImportMulti(MultiOptions),
    #[command(name = "reconcile-blocks")]
    ReconcileBlocks(ReconcileOptions),
}

#[derive(Debug, Args)]
struct ReconcileOptions {
    #[clap(long)]
    batch_blocks: i64,
}

#[derive(Debug, Args)]
struct MultiOptions {
    #[clap(long)]
    nr_threads: i64,

    #[clap(long)]
    batch_blocks: i64,
}

#[derive(Debug, Args)]
struct ImportOptions {
    #[clap(long)]
    nr_machines: i64,

    #[clap(long)]
    batch_blocks: i64,

    #[clap(long)]
    machine: i64,

    #[clap(long)]
    nr_batches: Option<i64>,
}

async fn download_persistence(download_dir: &str, unpack_dir: &str) -> Result<()> {
    let ctx = Context::new(
        "301978b4-0c0a-4b6b-ad7b-3a2f63c5182c",
        "testnet-901",
        &download_dir,
    )
    .await?;
    println!("Downloading persistence to {}", download_dir);
    let historical = Historical::new(&ctx)?;
    historical.download().await?;
    println!("Download persistence .. ");
    let incr = Incremental::new(&ctx)?;
    let max_block = incr.get_max_block().await?;
    incr.download_persistence().await?;
    incr.download_incr_persistence().await?;
    incr.download_incr_state().await?;
    incr.save_meta(max_block)?;
    println!("Max block {}", max_block);

    let render = Renderer::new("testnet-901", download_dir, unpack_dir)?;
    let recovery_points = render.get_recovery_points()?;
    println!(
        "persistence blocks: {:?} , state blocks: {:?}",
        recovery_points.persistence_blocks, recovery_points.state_delta_blocks
    );
    println!("RP : {:?} ", recovery_points.recovery_points);

    render.unpack(&recovery_points, None)?;

    println!("Persistence was rendered successfully");
    Ok(())
}

async fn dump_persistence(unpack_dir: &str) -> Result<()> {
    let exporter = Exporter::new(&unpack_dir)?;
    let max_block = exporter.get_max_block();
    println!("max_block is {}", max_block);

    // exporter.import_txns(4, max_block, None);
    for val in exporter.micro_blocks(1, max_block, None)? {
        if let Ok((key, blk)) = val {
            println!("Blk! at {}/{}", key.epochnum, key.shardid);
            for (_hash, maybe_txn) in exporter.txns(&key, &blk)? {
                if let Some(txn) = maybe_txn {
                    if let Some(actually_txn) = txn.transaction {
                        println!("Txn {}", H256::from_slice(&actually_txn.tranid));
                    }
                }
            }
        } else {
            println!("Error!");
        }
    }
    Ok(())
}

async fn bigquery_import_multi(unpack_dir: &str, opts: &MultiOptions) -> Result<()> {
    bqimport::multi(unpack_dir, opts.nr_threads, opts.batch_blocks).await
}

async fn bigquery_import(unpack_dir: &str, opts: &ImportOptions) -> Result<()> {
    bqimport::import(
        unpack_dir,
        opts.nr_machines,
        opts.machine,
        opts.batch_blocks,
        opts.nr_batches,
    )
    .await
}

async fn bigquery_reconcile_blocks(unpack_dir: &str, opts: &ReconcileOptions) -> Result<()> {
    bqimport::reconcile_blocks(unpack_dir, opts.batch_blocks).await
}

#[tokio::main]
async fn main() -> Result<()> {
    let cli = Cli::parse();

    match &cli.cmd {
        Commands::DownloadPersistence => {
            download_persistence(&cli.download_dir, &cli.unpack_dir).await
        }
        Commands::DumpPersistence => dump_persistence(&cli.unpack_dir).await,
        Commands::ImportBq(opts) => bigquery_import(&cli.unpack_dir, opts).await,
        Commands::ImportMulti(opts) => bigquery_import_multi(&cli.unpack_dir, opts).await,
        Commands::ReconcileBlocks(opts) => bigquery_reconcile_blocks(&cli.unpack_dir, opts).await,
    }
}
