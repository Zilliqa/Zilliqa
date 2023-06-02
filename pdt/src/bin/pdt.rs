use anyhow::Result;
use pdtlib::context::Context;
use pdtlib::exporter::Exporter;
use pdtlib::historical::Historical;
use pdtlib::incremental::Incremental;
use pdtlib::render::Renderer;
use primitive_types::H256;

#[tokio::main]
async fn main() -> Result<()> {
    //let ctx = Context::new(
    //"c5b68604-8540-4887-ad29-2ab9e680f997",
    //"mainnet-v901",
    //"/tmp/test",
    //).await?;

    let download_dir = "/home/rrw/tmp/test";
    let unpack_dir = "/home/rrw/tmp/unpacked";
    let ctx = Context::new(
        "301978b4-0c0a-4b6b-ad7b-3a2f63c5182c",
        "testnet-901",
        &download_dir,
    )
    .await?;
    if (false) {
        println!("Download history ..");
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

        println!("Rendered .. ");
    }
    // OK. Now.
    let exporter = Exporter::new(&unpack_dir)?;
    let max_block = exporter.get_max_block();
    println!("max_block is {}", max_block);

    // exporter.import_txns(4, max_block, None);
    for val in exporter.micro_blocks(1, max_block, None)? {
        if let Ok((key, blk)) = val {
            println!("Blk! at {}/{}", key.epochnum, key.shardid);
            for (hash, maybe_txn) in exporter.txns(&key, &blk)? {
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

    println!("Hello, pdt!");
    Ok(())
}
