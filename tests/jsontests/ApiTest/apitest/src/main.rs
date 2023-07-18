use anyhow::{anyhow, Result};
use clap::Parser;
use reqwest;
use serde::{de::DeserializeOwned, Deserialize, Serialize};
use std::assert;
use std::env;

#[derive(Parser, Debug)]
#[clap(about)]
struct Cli {
    #[clap(value_name = "url")]
    pub url: String,
}

// There are quite a lot of "nice" things you can do here with serde_json, but since what we're trying to
// test is the raw requests, I've not done them. As the test suite expands, we probably should.

pub struct Response {
    pub status: reqwest::StatusCode,
    pub body: String,
}

impl Response {
    pub fn new(status: &reqwest::StatusCode, body: &str) -> Self {
        Self {
            status: status.clone(),
            body: body.to_string(),
        }
    }
}

struct Client {
    pub url: String,
}

impl Client {
    pub fn new(url: &str) -> Result<Self> {
        Ok(Self {
            url: url.to_string(),
        })
    }

    pub async fn with_body(&self, body: &str) -> Result<Response> {
        let client = reqwest::Client::new();
        println!(">> {0}", body);
        let result = client
            .post(&self.url)
            .body(body.to_string())
            .header("Content-Type", "application/json")
            .send()
            .await?;
        let status = result.status();
        println!("<< {0}", status.as_u16());
        let text_result = result.text().await?.clone();
        println!("<< {0}", &text_result);
        Ok(Response::new(&status, &text_result))
    }

    pub async fn with_request(&self, a: JsonRPCRequest) -> Result<Response> {
        let body = serde_json::to_string(&a)?;
        Ok(self.with_body(&body).await?)
    }

    pub async fn with_rpc<B>(
        &self,
        a: JsonRPCRequest,
    ) -> Result<(reqwest::StatusCode, JsonRpcResponse<B>)>
    where
        B: DeserializeOwned,
        B: Clone,
    {
        let response = self.with_request(a).await?;
        let code = response.status;
        let rpc_result: JsonRpcResponse<B> = serde_json::from_str(&response.body).map_err(|e| {
            anyhow!(format!(
                "Response text {0} was not a valid JSON-RPC response - {1:?}",
                &response.body, e
            ))
        })?;
        Ok((code, rpc_result))
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct RpcError {
    pub code: i64,
    pub message: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct JsonRpcResponse<A: Clone> {
    pub id: i64,
    pub error: Option<RpcError>,
    pub jsonrpc: String,
    pub result: Option<A>,
}

impl<A: Clone> JsonRpcResponse<A> {
    pub fn is_result(&self) -> bool {
        self.result.is_some() && self.error.is_none()
    }

    pub fn is_error(&self) -> bool {
        self.error.is_some()
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct JsonRPCRequest {
    id: u32,
    jsonrpc: String,
    method: String,
    params: Vec<serde_json::Value>,
}

impl JsonRPCRequest {
    pub fn new(method: &str) -> Self {
        let params: Vec<serde_json::Value> = Vec::new();
        Self {
            id: 2,
            jsonrpc: "2.0".to_string(),
            method: method.to_string(),
            params,
        }
    }

    pub fn push<A: Serialize + Clone>(&mut self, a: &A) -> Result<&mut Self> {
        self.params.push(serde_json::value::to_value(a.clone())?);
        Ok(self)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct EmptyJsonRequest {}

fn int_from_string(in_string: &str) -> Result<i128> {
    Ok(parse_int::parse(in_string)?)
}

async fn test_block_number(client: &Client) -> Result<()> {
    let req = JsonRPCRequest::new("eth_blockNumber");
    // I have become jamesh, destroyer of minds.
    let result = client.with_rpc::<String>(req).await?;
    assert!(result.0.is_success());
    assert!(result.1.is_result());
    let blk = int_from_string(&result.1.result.unwrap())?;
    assert!(blk > 0);
    Ok(())
}

async fn test_eth_estimate_gas(client: &Client) -> Result<()> {
    #[derive(Clone, Serialize)]
    struct Request {
        data: String,
        from: String,
        value: String,
    }

    let req = Request {
        data: "0x60806040526040516105d83803806105d8833981810160405281019061002591906100f0565b804210610067576040517f08c379a000000000000000000000000000000000000000000000000000000000815260040161005e906101a0565b60405180910390fd5b8060008190555033600160006101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff160217905550506101c0565b600080fd5b6000819050919050565b6100cd816100ba565b81146100d857600080fd5b50565b6000815190506100ea816100c4565b92915050565b600060208284031215610106576101056100b5565b5b6000610114848285016100db565b91505092915050565b600082825260208201905092915050565b7f556e6c6f636b2074696d652073686f756c6420626520696e207468652066757460008201527f7572650000000000000000000000000000000000000000000000000000000000602082015250565b600061018a60238361011d565b91506101958261012e565b604082019050919050565b600060208201905081810360008301526101b98161017d565b9050919050565b610409806101cf6000396000f3fe608060405234801561001057600080fd5b50600436106100415760003560e01c8063251c1aa3146100465780633ccfd60b146100645780638da5cb5b1461006e575b600080fd5b61004e61008c565b60405161005b919061024a565b60405180910390f35b61006c610092565b005b61007661020b565b60405161008391906102a6565b60405180910390f35b60005481565b6000544210156100d7576040517f08c379a00000000000000000000000000000000000000000000000000000000081526004016100ce9061031e565b60405180910390fd5b600160009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff163373ffffffffffffffffffffffffffffffffffffffff1614610167576040517f08c379a000000000000000000000000000000000000000000000000000000000815260040161015e9061038a565b60405180910390fd5b7fbf2ed60bd5b5965d685680c01195c9514e4382e28e3a5a2d2d5244bf59411b9347426040516101989291906103aa565b60405180910390a1600160009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166108fc479081150290604051600060405180830381858888f19350505050158015610208573d6000803e3d6000fd5b50565b600160009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1681565b6000819050919050565b61024481610231565b82525050565b600060208201905061025f600083018461023b565b92915050565b600073ffffffffffffffffffffffffffffffffffffffff82169050919050565b600061029082610265565b9050919050565b6102a081610285565b82525050565b60006020820190506102bb6000830184610297565b92915050565b600082825260208201905092915050565b7f596f752063616e27742077697468647261772079657400000000000000000000600082015250565b60006103086016836102c1565b9150610313826102d2565b602082019050919050565b60006020820190508181036000830152610337816102fb565b9050919050565b7f596f75206172656e277420746865206f776e6572000000000000000000000000600082015250565b60006103746014836102c1565b915061037f8261033e565b602082019050919050565b600060208201905081810360008301526103a381610367565b9050919050565b60006040820190506103bf600083018561023b565b6103cc602083018461023b565b939250505056fea264697066735822122037d72a62344bd1b2480de1f3f4d6ffe4a35d6a5337d4c346f069eed9df11cad164736f6c634300081300330000000000000000000000000000000000000000000000000000000064b6ec09".to_string(),
        from: "0xf0cb24ac66ba7375bf9b9c4fa91e208d9eaabd2e".to_string(),
        value: "0x38d7ea4c68000".to_string() };

    // Check that this works with and without the block number argument.
    {
        let mut rpc = JsonRPCRequest::new("eth_estimateGas");
        rpc.push(&req)?;
        let result = client.with_rpc::<String>(rpc).await?;
        assert!(result.0.is_success());
        if result.1.is_result() {
            assert!(result.1.is_result());
            // Don't care what the int is, just that it exists.
            let _ = int_from_string(&result.1.result.unwrap())?;
        } else {
            // "Unlock time must be in the future"
            assert_eq!(result.1.error.unwrap().code, 3);
        }
    }
    // Now with a block index
    {
        let mut rpc = JsonRPCRequest::new("eth_estimateGas");
        rpc.push(&req)?;
        rpc.push(&"pending".to_string())?;
        let result = client.with_rpc::<String>(rpc).await?;
        assert!(result.0.is_success());
        if result.1.is_result() {
            assert!(result.1.is_result());
            // Don't care what the int is, just that it exists.
            let _ = int_from_string(&result.1.result.unwrap())?;
        } else {
            // "Unlock time must be in the future"
            assert_eq!(result.1.error.unwrap().code, 3);
        }
    }

    // Now with an invalid block index
    {
        let mut rpc = JsonRPCRequest::new("eth_estimateGas");
        rpc.push(&req)?;
        rpc.push(&"wombats".to_string())?;
        let result = client.with_rpc::<String>(rpc).await?;
        // This is an error due to the invalid block index.
        assert!(result.0.is_success());
        assert!(result.1.is_error());
        assert!(result.1.error.unwrap().code == -32602);
    }

    Ok(())
}

/// A very trivial test program which takes a series of request/response pairs and pattern-matches them.
#[tokio::main]
async fn main() -> Result<()> {
    let cli = Cli::parse_from(env::args());
    let client = Client::new(&cli.url)?;

    test_block_number(&client).await?;
    test_eth_estimate_gas(&client).await?;
    println!("Test passed!");
    Ok(())
}
