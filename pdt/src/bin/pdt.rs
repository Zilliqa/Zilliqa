use eyre::Result;
use pdtlib::context::Context;
use pdtlib::historical::Historical;
use pdtlib::incremental::Incremental;
use pdtlib::render::Renderer;

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
    println!("Download history ..");
    let historical = Historical::new(&ctx)?;
    historical.download().await?;
    println!("Download persistence .. ");
    let incr = Incremental::new(&ctx)?;
    incr.download_persistence().await?;
    incr.download_incr_persistence().await?;
    incr.download_incr_state().await?;
    println!("Max block {}", incr.get_max_block().await?);

    let render = Renderer::new("testnet-901", download_dir, unpack_dir)?;
    let recovery_points = render.get_recovery_points()?;
    println!(
        "persistence blocks: {:?} , state blocks: {:?}",
        recovery_points.persistence_blocks, recovery_points.state_delta_blocks
    );
    println!("RP : {:?} ", recovery_points.recovery_points);

    render.unpack(&recovery_points, None)?;

    println!("Hello, pdt!");
    Ok(())
}
