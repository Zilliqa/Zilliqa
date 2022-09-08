//! Implementation of EVM for Zilliqa

// #![deny(warnings)]
#![forbid(unsafe_code)]

mod ipc_connect;
mod precompiles;
mod protos;
mod scillabackend;

use std::collections::BTreeMap;
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use std::panic::{self, AssertUnwindSafe};
use std::path::PathBuf;
use std::rc::Rc;
use std::sync::{Arc, Mutex};

use clap::Parser;
use evm::{
    backend::{Apply, Basic},
    executor::stack::{MemoryStackState, PrecompileFn, StackSubstateMetadata},
    tracing,
};

use serde::ser::{Serialize, SerializeStructVariant, Serializer};

use core::str::FromStr;
use log::{debug, error, info};

use jsonrpc_core::{BoxFuture, Error, IoHandler, Result};
use jsonrpc_derive::rpc;
use jsonrpc_server_utils::codecs;
use primitive_types::*;
use scillabackend::{ScillaBackend, ScillaBackendConfig};

/// EVM JSON-RPC server
#[derive(Parser, Debug)]
#[clap(version, about, long_about = None)]
struct Args {
    /// Path of the EVM server Unix domain socket.
    #[clap(short, long, default_value = "/tmp/evm-server.sock")]
    socket: String,

    /// Path of the Node Unix domain socket.
    #[clap(short, long, default_value = "/tmp/zilliqa.sock")]
    node_socket: String,

    /// Path of the EVM server HTTP socket. Duplicates the `socket` above for convenience.
    #[clap(short = 'p', long, default_value = "3333")]
    http_port: u16,

    /// Trace the execution with debug logging.
    #[clap(short, long)]
    tracing: bool,

    /// Log file (if not set, stderr is used).
    #[clap(short, long)]
    log4rs: Option<String>,

    /// How much EVM gas is one Scilla gas worth.
    #[clap(long, default_value = "1")]
    gas_scaling_factor: u64,

    /// Zil scaling factor.  How many Zils in one EVM visible Eth.
    #[clap(long, default_value = "1")]
    zil_scaling_factor: u64,
}

struct DirtyState(Apply<Vec<(String, String)>>);

impl Serialize for DirtyState {
    fn serialize<S>(&self, serializer: S) -> std::result::Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match &self.0 {
            Apply::Modify {
                ref address,
                ref basic,
                ref code,
                ref storage,
                reset_storage,
            } => {
                let mut state = serializer.serialize_struct_variant("A", 0, "modify", 6)?;
                state.serialize_field("address", address)?;
                state.serialize_field("balance", &basic.balance)?;
                state.serialize_field("nonce", &basic.nonce)?;
                state.serialize_field("code", &code.as_ref().map(hex::encode))?;
                state.serialize_field("storage", storage)?;
                state.serialize_field("reset_storage", &reset_storage)?;
                Ok(state.end()?)
            }
            Apply::Delete { address } => {
                let mut state = serializer.serialize_struct_variant("A", 0, "delete", 1)?;
                state.serialize_field("address", address)?;
                Ok(state.end()?)
            }
        }
    }
}

#[derive(serde::Serialize)]
pub struct EvmResult {
    exit_reason: evm::ExitReason,
    return_value: String,
    apply: Vec<DirtyState>,
    logs: Vec<ethereum::Log>,
    remaining_gas: u64,
}

#[rpc(server)]
pub trait Rpc: Send + 'static {
    #[rpc(name = "run")]
    fn run(
        &self,
        address: String,
        caller: String,
        code: String,
        data: String,
        apparent_value: String,
        gas_limit: u64,
    ) -> BoxFuture<Result<EvmResult>>;
}

struct EvmServer {
    tracing: bool,
    backend_config: ScillaBackendConfig,
    gas_scaling_factor: u64,
}

impl Rpc for EvmServer {
    fn run(
        &self,
        address: String,
        caller: String,
        code_hex: String,
        data_hex: String,
        apparent_value: String,
        gas_limit: u64,
    ) -> BoxFuture<Result<EvmResult>> {
        let backend = ScillaBackend::new(self.backend_config.clone());
        let tracing = self.tracing;
        let gas_scaling_factor = self.gas_scaling_factor;
        Box::pin(async move {
            run_evm_impl(
                address,
                caller,
                code_hex,
                data_hex,
                apparent_value,
                gas_limit,
                backend,
                tracing,
                gas_scaling_factor,
            )
            .await
        })
    }
}

#[allow(clippy::too_many_arguments)]
async fn run_evm_impl(
    address: String,
    caller: String,
    code_hex: String,
    data_hex: String,
    apparent_value: String,
    gas_limit: u64,
    backend: ScillaBackend,
    tracing: bool,
    gas_scaling_factor: u64,
) -> Result<EvmResult> {
    // We must spawn a separate blocking task (on a blocking thread), because by default a JSONRPC
    // method runs as a non-blocking thread under a tokio runtime, and creating a new runtime
    // cannot be done. And we'll need a new runtime that we can safely drop on a handled
    // panic. (Using the parent runtime and dropping on stack unwind will mess up the parent
    // runtime).
    tokio::task::spawn_blocking(move || {
        let code =
            Rc::new(hex::decode(&code_hex).map_err(|e| {
                Error::invalid_params(format!("code: '{}...' {}", &code_hex[..10], e))
            })?);
        let data =
            Rc::new(hex::decode(&data_hex).map_err(|e| {
                Error::invalid_params(format!("data: '{}...' {}", &data_hex[..10], e))
            })?);

        let config = evm::Config::london();
        let apparent_value = U256::from_dec_str(&apparent_value)
            .map_err(|e| Error::invalid_params(format!("apparent_value: {}", e)))?;
        let context = evm::Context {
            address: H160::from_str(&address)
                .map_err(|e| Error::invalid_params(format!("address: {}", e)))?,
            caller: H160::from_str(&caller)
                .map_err(|e| Error::invalid_params(format!("caller: {}", e)))?,
            apparent_value,
        };
        let mut runtime = evm::Runtime::new(code, data, context, &config);
        // Scale the gas limit.
        let gas_limit = gas_limit * gas_scaling_factor;
        let metadata = StackSubstateMetadata::new(gas_limit, &config);
        let state = MemoryStackState::new(metadata, &backend);

        // TODO: implement all precompiles.
        let precompiles = BTreeMap::from([(
            H160::from_str("0000000000000000000000000000000000000001").unwrap(),
            precompiles::ecrecover as PrecompileFn,
        )]);

        let mut executor =
            evm::executor::stack::StackExecutor::new_with_precompiles(state, &config, &precompiles);

        info!(
            "Executing runtime with code \"{:?}\" and data \"{:?}\"",
            code_hex, data_hex,
        );
        let mut listener = LoggingEventListener;

        // We have to catch panics, as error handling in the Backend interface of
        // do not have Result, assuming all operations are successful.
        //
        // We are asserting it is safe to unwind, as objects will be dropped after
        // the unwind.
        let result = panic::catch_unwind(AssertUnwindSafe(|| {
            if tracing {
                evm::tracing::using(&mut listener, || executor.execute(&mut runtime))
            } else {
                executor.execute(&mut runtime)
            }
        }));
        // Scale back remaining gas to Scilla units (no rounding!).
        let remaining_gas = executor.gas() / gas_scaling_factor;
        match result {
            Ok(exit_reason) => {
                info!("Exit: {:?}", exit_reason);
                let (state_apply, logs) = executor.into_state().deconstruct();
                info!(
                    "Return value: {:?}",
                    hex::encode(runtime.machine().return_value())
                );
                Ok(EvmResult {
                    exit_reason,
                    return_value: hex::encode(runtime.machine().return_value()),
                    apply: state_apply
                        .into_iter()
                        .map(|apply| match apply {
                            Apply::Delete { address } => DirtyState(Apply::Delete { address }),
                            Apply::Modify {
                                address,
                                basic,
                                code,
                                storage,
                                reset_storage,
                            } => DirtyState(Apply::Modify {
                                address,
                                basic: Basic {
                                    balance: backend.scale_eth_to_zil(basic.balance),
                                    nonce: basic.nonce,
                                },
                                code,
                                storage: storage
                                    .into_iter()
                                    .map(|(k, v)| backend.encode_storage(k, v))
                                    .collect(),
                                reset_storage,
                            }),
                        })
                        .collect(),
                    logs: logs.into_iter().collect(),
                    remaining_gas,
                })
            }
            Err(panic) => {
                let panic_message = panic
                    .downcast::<String>()
                    .unwrap_or(Box::new("unknown panic".to_string()));
                error!("EVM panicked: '{:?}'", panic_message);
                Ok(EvmResult {
                    exit_reason: evm::ExitReason::Fatal(evm::ExitFatal::Other(
                        format!("EVM execution failed: '{:?}'", panic_message).into(),
                    )),
                    return_value: "".to_string(),
                    apply: vec![],
                    logs: vec![], // TODO: shouldn't we get the logs here too?
                    remaining_gas,
                })
            }
        }
    })
    .await
    .unwrap()
}

struct LoggingEventListener;

impl tracing::EventListener for LoggingEventListener {
    fn event(&mut self, event: tracing::Event) {
        debug!("EVM Event {:?}", event);
    }
}

fn main() -> std::result::Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();

    match args.log4rs {
        Some(log_config) if log_config != "" => {
            log4rs::init_file(log_config, Default::default()).unwrap();
        }
        _ => {
            let config_str = include_str!("../log4rs-local.yml");
            let config = serde_yaml::from_str(config_str).unwrap();
            log4rs::init_raw_config(config).unwrap();
        }
    }

    info!("Starting evm-ds");

    let evm_sever = EvmServer {
        tracing: args.tracing,
        backend_config: ScillaBackendConfig {
            path: PathBuf::from(args.node_socket),
            zil_scaling_factor: args.zil_scaling_factor,
        },
        gas_scaling_factor: args.gas_scaling_factor,
    };

    // Setup a channel to signal a shutdown.
    let (shutdown_sender, shutdown_receiver) = std::sync::mpsc::channel();

    let mut io = IoHandler::new();
    io.extend_with(evm_sever.to_delegate());
    let shutdown_sender = std::sync::Mutex::new(shutdown_sender);
    // Have the "die" method send a signal to shut it down.
    // Mutex because the methods require all captured values to be Sync.
    // Set up a channel to shut down the servers
    io.add_method("die", move |_param| {
        let _ = shutdown_sender.lock().unwrap().send(()).unwrap();
        futures::future::ready(Ok(jsonrpc_core::Value::Null))
    });

    let ipc_server_handle: Arc<Mutex<Option<jsonrpc_ipc_server::CloseHandle>>> =
        Arc::new(Mutex::new(None));
    let ipc_server_handle_clone = ipc_server_handle.clone();
    let http_server_handle: Arc<Mutex<Option<jsonrpc_http_server::CloseHandle>>> =
        Arc::new(Mutex::new(None));
    let http_server_handle_clone = http_server_handle.clone();

    // Build and start the IPC server (Unix domain socket).
    let builder = jsonrpc_ipc_server::ServerBuilder::new(io.clone()).request_separators(
        codecs::Separator::Byte(b'\n'),
        codecs::Separator::Byte(b'\n'),
    );
    let ipc_server = builder.start(&args.socket).expect("Couldn't open socket");
    // Save the handle so that we can shut it down gracefully.
    *ipc_server_handle.lock().unwrap() = Some(ipc_server.close_handle());

    // Build and start the HTTP server.
    let builder = jsonrpc_http_server::ServerBuilder::new(io);
    let http_server = builder
        .start_http(&SocketAddr::new(
            IpAddr::V4(Ipv4Addr::new(127, 0, 0, 1)),
            args.http_port,
        ))
        .expect("Couldn't open socket");
    // Save the handle so that we can shut it down gracefully.
    *http_server_handle.lock().unwrap() = Some(http_server.close_handle());

    // At this point, both servers are running on separate threads with own tokio runtimes.
    // Here we only wait until a shutdown signal comes.
    let _ = shutdown_receiver.recv();

    // Send signals to each of the servers to shut down.
    if let Some(handle) = ipc_server_handle_clone.lock().unwrap().take() {
        handle.close()
    }
    if let Some(handle) = http_server_handle_clone.lock().unwrap().take() {
        handle.close()
    }

    // Wait until both servers shutdown cleanly.
    ipc_server.wait();
    http_server.wait();

    Ok(())
}
