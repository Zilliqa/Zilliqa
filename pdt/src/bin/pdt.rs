use eyre::Result;
use pdtlib::context::Context;
use pdtlib::historical::Historical;
use pdtlib::incremental::Incremental;

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
    //let historical = Historical::new(&ctx)?;
    //    historical.download().await?;
    let incremental = Incremental::new(&ctx)?;
    println!("Max block {}", incremental.get_max_block().await?);
    println!("Hello, pdt!");
    Ok(())
}
