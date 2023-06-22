/// Backend implementation that stores EVM state via the Scilla JSONRPC interface.
use std::path::PathBuf;
use std::str::FromStr;

use bytes::Bytes;
use evm::backend::{Backend, Basic};
use jsonrpc_core::serde_json;
use jsonrpc_core::types::params::Params;
use jsonrpc_core::{Error, Result, Value};
use jsonrpc_core_client::RawClient;
use primitive_types::{H160, H256, U256};

use log::debug;

use protobuf::Message;

use crate::ipc_connect;
use crate::protos::Evm as EvmProto;
use crate::protos::ScillaMessage;

#[derive(Clone)]
pub struct ScillaBackendConfig {
    // Path to the Unix domain socket over which we talk to the Node.
    pub path: PathBuf,
    // Scaling factor of Eth <-> Zil. Should be either 1 or 1_000_000.
    pub zil_scaling_factor: u64,
}

// Backend relying on Scilla variables and Scilla JSONRPC interface.
pub struct ScillaBackend {
    pub config: ScillaBackendConfig,
    pub origin: H160,
    pub extras: EvmProto::EvmEvalExtras,
}

// Adding some convenience to ProtoScillaVal to convert to U256 and bytes.
impl ScillaMessage::ProtoScillaVal {
    fn as_uint256(&self) -> Option<U256> {
        // Parse the way  ContractStorage::FetchExternalStateValue encodes it.
        String::from_utf8(self.get_bval().to_vec())
            .ok()
            .and_then(|s| {
                let s = s.replace('\"', "");
                if let Some(stripped) = s.strip_prefix("0x") {
                    U256::from_str(stripped).ok()
                } else {
                    U256::from_dec_str(&s).ok()
                }
            })
    }

    fn as_bytes(&self) -> Vec<u8> {
        Vec::from(self.get_bval())
    }
}

impl ScillaBackend {
    pub fn new(config: ScillaBackendConfig, origin: H160, extras: EvmProto::EvmEvalExtras) -> Self {
        Self {
            config,
            origin,
            extras,
        }
    }

    // Call the Scilla IPC Server API.
    fn call_ipc_server_api(
        &self,
        method: &str,
        args: serde_json::Map<String, Value>,
    ) -> Result<Value> {
        debug!("call_ipc_server_api: {}, {:?}", method, args);
        // Within this runtime, we need a separate runtime just to handle all JSON
        // client operations. The runtime will then drop and close all connections
        // and release all resources. Also when the thread panics.
        let rt = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .unwrap();
        let call_with_timeout = rt.block_on(async move {
            let client: RawClient = ipc_connect::ipc_connect(&self.config.path)
                .await
                .expect("Failed to connect to the node Unix domain socket");
            tokio::time::timeout(
                tokio::time::Duration::from_secs(10), // Require response in 10 secs max.
                client.call_method(method, Params::Map(args)),
            )
            .await
        });
        if let Ok(result) = call_with_timeout {
            result.map_err(|_| Error::internal_error())
        } else {
            panic!("timeout calling {method}");
        }
    }

    fn query_jsonrpc(&self, query_name: &str, query_args: Option<&str>) -> Value {
        debug!("query_jsonrpc: {}, {:?}", query_name, query_args);
        // Make a JSON Query for fetchBlockchaininfo
        let mut args = serde_json::Map::new();
        args.insert("query_name".into(), query_name.into());
        args.insert("query_args".into(), query_args.unwrap_or_default().into());
        let result = self
            .call_ipc_server_api("fetchBlockchainInfo", args)
            .unwrap_or_default();
        // Check that the call succeeded.
        let null = Value::Null;
        let succeeded = result.get(0).unwrap_or(&null).as_bool().unwrap_or_default();

        if succeeded {
            // Check that there is a result of a given type.
            result.get(1).unwrap_or(&null).clone()
        } else {
            null
        }
    }

    fn query_state_value(
        &self,
        address: H160,
        query_name: &str,
        key: Option<H256>,
        use_default: bool,
    ) -> Result<Option<ScillaMessage::ProtoScillaVal>> {
        debug!(
            "query_state_value: {} {} {:?} {}",
            address, query_name, key, use_default
        );
        let mut query = ScillaMessage::ProtoScillaQuery::new();
        query.set_name(query_name.into());
        if let Some(key) = key {
            query.set_indices(vec![bytes::Bytes::from(format!("{key:X}"))]);
            query.set_mapdepth(1);
        } else {
            query.set_mapdepth(0);
        }

        let mut args = serde_json::Map::new();
        args.insert("addr".into(), hex::encode(address.as_bytes()).into());
        args.insert(
            "query".into(),
            base64::encode(query.write_to_bytes().unwrap()).into(),
        );

        // If the RPC call failed, something is wrong, and it is better to crash.
        let result = self.call_ipc_server_api("fetchExternalStateValueB64", args);
        // If the RPC was okay, but we didn't get a value, that's
        // normal, just return empty code.
        match result {
            Ok(result) => {
                let default_false = Value::Bool(false);
                if !result
                    .get(0)
                    .map_or_else(
                        || {
                            if use_default {
                                Ok(&default_false)
                            } else {
                                Err(Error::internal_error())
                            }
                        },
                        Ok,
                    )?
                    .as_bool()
                    .unwrap_or_default()
                {
                    return Ok(None);
                }

                // Check that there is a result of a given type.
                let default_value = ScillaMessage::ProtoScillaVal::new();
                result.get(1).map_or_else(
                    || {
                        if use_default {
                            Ok(Some(default_value))
                        } else {
                            Err(Error::internal_error())
                        }
                    },
                    |value| {
                        value
                            .as_str()
                            .map(|value_str| {
                                base64::decode(value_str).ok().and_then(|buffer| {
                                    ScillaMessage::ProtoScillaVal::parse_from_bytes(&buffer).ok()
                                })
                            })
                            .ok_or_else(Error::internal_error)
                    },
                )
            }
            Err(_) => Ok(None),
        }
    }

    // Encode key/value pairs for storage in such a way that the Zilliqa node
    // could interpret it without much modification.
    #[allow(dead_code)]
    pub(crate) fn encode_storage(&self, key: H256, value: H256) -> (Bytes, Bytes) {
        let mut query = ScillaMessage::ProtoScillaQuery::new();
        query.set_name("_evm_storage".into());
        query.set_indices(vec![bytes::Bytes::from(format!("{key:X}"))]);
        query.set_mapdepth(1);
        let mut val = ScillaMessage::ProtoScillaVal::new();
        let bval = value.as_bytes().to_vec();
        val.set_bval(bval.into());
        (
            query.write_to_bytes().unwrap().into(),
            val.write_to_bytes().unwrap().into(),
        )
    }
}

#[allow(dead_code)]
pub(crate) fn scale_eth_to_zil(eth: U256, zil_scaling_factor: u64) -> U256 {
    eth / zil_scaling_factor
}

pub(crate) fn scale_zil_to_eth(zil: U256, zil_scaling_factor: u64) -> U256 {
    zil * zil_scaling_factor
}

pub(crate) fn encode_storage(key: H256, value: H256, for_z1: bool) -> (Bytes, Bytes) {
    if for_z1 {
        let mut query = ScillaMessage::ProtoScillaQuery::new();
        query.set_name("_evm_storage".into());
        query.set_indices(vec![bytes::Bytes::from(format!("{key:X}"))]);
        query.set_mapdepth(1);
        let mut val = ScillaMessage::ProtoScillaVal::new();
        let bval = value.as_bytes().to_vec();
        val.set_bval(bval.into());
        (
            query.write_to_bytes().unwrap().into(),
            val.write_to_bytes().unwrap().into(),
        )
    } else {
        (
            Bytes::copy_from_slice(key.as_bytes()),
            Bytes::copy_from_slice(value.as_bytes()),
        )
    }
}

impl Backend for ScillaBackend {
    fn gas_price(&self) -> U256 {
        self.extras.get_gas_price().into()
    }

    fn origin(&self) -> H160 {
        self.origin
    }

    fn block_hash(&self, number: U256) -> H256 {
        let result = self.query_jsonrpc("BLOCKHASH", Some(&number.to_string()));
        H256::from_str(result.as_str().expect("blockhash")).expect("blockhash hex")
    }

    fn block_number(&self) -> U256 {
        self.extras.get_block_number().into()
    }

    fn block_coinbase(&self) -> H160 {
        // TODO: implement according to the logic of Zilliqa.
        H160::zero()
    }

    fn block_timestamp(&self) -> U256 {
        self.extras.get_block_timestamp().into()
    }

    fn block_difficulty(&self) -> U256 {
        self.extras.get_block_difficulty().into()
    }

    fn block_gas_limit(&self) -> U256 {
        self.extras.get_block_gas_limit().into()
    }

    fn block_base_fee_per_gas(&self) -> U256 {
        // TODO: Implement EIP-1559
        // For now just return the gas price.
        self.gas_price()
    }

    fn chain_id(&self) -> U256 {
        self.extras.get_chain_id().into()
    }

    fn exists(&self, address: H160) -> bool {
        // Try to query account balance, and see if it returns Some result.
        self.query_state_value(address, "_balance", None, true)
            .expect("query_state_value _balance")
            .is_some()
    }

    fn basic(&self, address: H160) -> Basic {
        let balance = self
            .query_state_value(address, "_balance", None, true)
            .expect("query_state_value _balance")
            .and_then(|x| x.as_uint256())
            .unwrap_or_default();
        let nonce = self
            .query_state_value(address, "_nonce", None, true)
            .expect("query_state_value _nonce")
            .and_then(|x| x.as_uint256())
            .unwrap_or_default();
        Basic {
            balance: scale_zil_to_eth(balance, self.config.zil_scaling_factor),
            nonce,
        }
    }

    fn code(&self, address: H160) -> Vec<u8> {
        let bytes = self
            .query_state_value(address, "_code", None, true)
            .expect("query_state_value(_code)")
            .map(|value| value.as_bytes())
            .unwrap_or_default();
        if bytes.len() > 2 && bytes[0] == b'E' && bytes[1] == b'V' && bytes[2] == b'M' {
            bytes[3..].to_vec()
        } else {
            bytes
        }
    }

    fn code_as_json(&self, address: H160) -> Vec<u8> {
        let mut query = ScillaMessage::ProtoScillaQuery::new();
        query.set_name("_code".into());
        let mut args = serde_json::Map::new();
        args.insert("addr".to_owned(), hex::encode(address.as_bytes()).into());
        args.insert(
            "query".into(),
            base64::encode(query.write_to_bytes().unwrap()).into(),
        );

        let Ok(result) = self.call_ipc_server_api("fetchCodeJson", args) else {
            return Vec::new()
        };
        serde_json::to_vec(&result).unwrap_or_default()
    }

    fn init_data_as_json(&self, address: H160) -> Vec<u8> {
        let mut query = ScillaMessage::ProtoScillaQuery::new();
        query.set_name("_code".into());
        let mut args = serde_json::Map::new();
        args.insert("addr".to_owned(), hex::encode(address.as_bytes()).into());
        args.insert(
            "query".into(),
            base64::encode(query.write_to_bytes().unwrap()).into(),
        );

        let Ok(result) = self.call_ipc_server_api("fetchContractInitDataJson", args) else {
            return Vec::new()
        };
        serde_json::to_vec(&result).unwrap_or_default()
    }

    fn substate_as_json(&self, address: H160, vname: &str, indices: &[String]) -> Vec<u8> {
        let mut args = serde_json::Map::new();
        args.insert("addr".to_owned(), hex::encode(address.as_bytes()).into());
        args.insert("vname".to_owned(), vname.into());
        args.insert("indices".to_owned(), indices.into());
        let Ok(result) = self.call_ipc_server_api("fetchStateJson", args) else {
            return Vec::new()
        };
        serde_json::to_vec(&result).unwrap_or_default()
    }

    fn storage(&self, address: H160, key: H256) -> H256 {
        let mut result = self
            .query_state_value(address, "_evm_storage", Some(key), true)
            .expect("query_state_value(_evm_storage)")
            .map(|value| value.as_bytes())
            .unwrap_or_default();
        // H256::from_slice expects big-endian, we filled the first bytes from decoding,
        // now need to extend to the required size.
        result.resize(256 / 8, 0u8);
        H256::from_slice(&result)
    }

    // We implement original_storage via storage, as we postpone writes until
    // contract commit time.
    fn original_storage(&self, address: H160, key: H256) -> Option<H256> {
        Some(self.storage(address, key))
    }
}
