use anyhow::Result;

pub struct ZQuery {
    pub client: Client,
}

impl ZQuery {
    pub fn new(url: &str, chain_id: u32) -> Result<ZQuery> {
        let client = Client::new(url, chain_id);
        Ok(ZQuery { client })
    }

    pub async fn retrieve_block(&self, block: i128) -> Result<()> {
        let txns = self
            .client
            .get_transactions_for_tx_block(&Integer::from(block))
            .await?;
        for maybe_shard in txns {
            if let Some(shard_vec) = maybe_shard {
                for txn_hash in shard_vec {
                    let hash_str = format!("{:x}", &txn_hash);
                    println!("Hash {}", hash_str);
                    match self.client.get_transaction(&hash_str).await {
                        Ok(txn) => println!("Got {:?}", txn),
                        Err(err) => println!("Failed - {}", err),
                    }
                }
            }
        }
        Ok(())
    }

    pub async fn scan_blocks(&self, since: i128, only_go_back: i128) -> Result<()> {
        let num_tx_blocks = *self.client.get_num_tx_blocks().await?.deref();
        let max_block = num_tx_blocks - only_go_back;
        let start_at = if since > max_block { since } else { max_block };
        println!(
            "num_tx {} max {} start{}",
            num_tx_blocks, max_block, start_at
        );
        for blk in start_at..num_tx_blocks {
            println!("Fetching {}", blk);
            let block = self.client.get_tx_block(&Integer::from(blk)).await;
            match block {
                Err(x) => println!("Error retrieving block {}", blk),
                Ok(block) => self.retrieve_block(blk).await?,
            }
        }
        Ok(())
    }
}
