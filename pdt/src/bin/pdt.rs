use eyre::Result;
use pdtlib::context::Context;
use pdtlib::historical::Historical;

#[tokio::main]
async fn main() -> Result<()> {
    let ctx = Context::new(
        "c5b68604-8540-4887-ad29-2ab9e680f997",
        "mainnet-v901",
        "/tmp/test",
    )
    .await?;
    let historical = Historical::new(&ctx)?;
    historical.download().await?;
    println!("Hello, pdt!");
    Ok(())
}
