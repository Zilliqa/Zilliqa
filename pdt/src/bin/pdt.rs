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

    let ctx = Context::new(
        "301978b4-0c0a-4b6b-ad7b-3a2f63c5182c",
        "testnet-901",
        "/tmp/test",
    )
    .await?;
    println!("Download history ..");
    let historical = Historical::new(&ctx)?;
    historical.download().await?;
    println!("Download persistence .. ");
    let incr = Incremental::new(&ctx)?;
    incr.download_persistence().await?;
    incr.download_incr_persistence().await?;
    println!("Max block {}", incr.get_max_block().await?);

    let render = Renderer::new("testnet-901", "/tmp/test", "/tmp/unpacked")?;
    let blocks = render.list_incrementals()?;
    println!("Incrementals : {:?} ", blocks);
    render.unpack(&blocks)?;

    println!("Hello, pdt!");
    Ok(())
}
