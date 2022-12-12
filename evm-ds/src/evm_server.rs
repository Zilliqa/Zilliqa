use futures::future::FutureExt;
use std::collections::HashMap;
use std::sync::{Arc, Mutex};

use jsonrpc_core::{BoxFuture, Error, Result};
use primitive_types::*;

type ContinuationId = usize;

use crate::continuation::Continuation;
use crate::evm_server_run::run_evm_impl;
use crate::protos::Evm as EvmProto;
use crate::scillabackend::{ScillaBackend, ScillaBackendConfig};
use protobuf::Message;

pub(crate) struct EvmServer {
    // Whether tracing is enabled for this instance of EVM server.
    tracing: bool,
    // Config for the backend that drives the interaction with the blockchain.
    backend_config: ScillaBackendConfig,
    // By how much to scale gas price.
    gas_scaling_factor: u64,
    // A cache of known continuations.
    continuations: Arc<Mutex<HashMap<ContinuationId, Continuation>>>,
}

impl EvmServer {
    pub fn new(
        tracing: bool,
        backend_config: ScillaBackendConfig,
        gas_scaling_factor: u64,
    ) -> Self {
        Self {
            tracing,
            backend_config,
            gas_scaling_factor,
            continuations: Arc::new(Mutex::new(HashMap::new())),
        }
    }

    pub fn run_json(&self, params: jsonrpc_core::Params) -> BoxFuture<Result<jsonrpc_core::Value>> {
        let args = jsonrpc_core::Value::from(params);
        if let Some(arg) = args.get(0) {
            if let Some(arg_str) = arg.as_str() {
                self.run(arg_str.to_string())
                    .map(|result| result.map(jsonrpc_core::Value::from))
                    .boxed()
            } else {
                futures::future::err(jsonrpc_core::Error::invalid_params("Invalid parameter"))
                    .boxed()
            }
        } else {
            futures::future::err(jsonrpc_core::Error::invalid_params("No parameter")).boxed()
        }
    }

    fn run(&self, args_str: String) -> BoxFuture<Result<String>> {
        let args_parsed = base64::decode(args_str)
            .map_err(|_| Error::invalid_params("cannot decode base64"))
            .and_then(|buffer| {
                EvmProto::EvmArgs::parse_from_bytes(&buffer)
                    .map_err(|e| Error::invalid_params(format!("{}", e)))
            });

        match args_parsed {
            Ok(mut args) => {
                let origin = H160::from(args.get_origin());
                let address = H160::from(args.get_address());
                let code = Vec::from(args.get_code());
                let data = Vec::from(args.get_data());
                let apparent_value = U256::from(args.get_apparent_value());
                let gas_limit = args.get_gas_limit();
                let estimate = args.get_estimate();
                let backend =
                    ScillaBackend::new(self.backend_config.clone(), origin, args.take_extras());
                let tracing = self.tracing;
                let gas_scaling_factor = self.gas_scaling_factor;
                let _continuation = args.take_continuation();

                run_evm_impl(
                    address,
                    code,
                    data,
                    apparent_value,
                    gas_limit,
                    backend,
                    tracing,
                    gas_scaling_factor,
                    estimate,
                    args.get_context().to_string(),
                )
                .boxed()
            }

            Err(e) => futures::future::err(e).boxed(),
        }
    }

    fn take_continuation_by_id(&self, continuation_id: ContinuationId) -> Option<Continuation> {
        if continuation_id > 0 {
            let mut continuations = self.continuations.lock().unwrap();
            continuations.remove(&continuation_id)
        } else {
            None
        }
    }
    /*/
    fn export_continuation(
        &self,
        _continuation_id: ContinuationId,
    ) -> Option<ContinuationSerialized> {
        // TODO: implement serializing and importing
        None
    }

    fn import_continuation(
        &mut self,
        _continuation_blob: ContinuationSerialized,
    ) -> Result<ContinuationId> {
        Err(Error::invalid_params(
            "continuation deserializaion not implemented",
        ))
    }
    */
}
