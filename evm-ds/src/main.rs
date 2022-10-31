//! Implementation of EVM for Zilliqa

// #![deny(warnings)]
#![forbid(unsafe_code)]

mod convert;
mod ipc_connect;
mod precompiles;
mod protos;
mod scillabackend;

use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use std::panic::{self, AssertUnwindSafe};
use std::path::PathBuf;
use std::rc::Rc;
use std::sync::{Arc, Mutex};

use anyhow::Context;
use clap::Parser;
use evm::{
    backend::Apply,
    executor::stack::{MemoryStackState, StackSubstateMetadata},
    tracing,
};
use futures::FutureExt;

use log::{error, info};
use std::fmt::Debug;

use jsonrpc_core::{BoxFuture, Error, IoHandler, Result};
use jsonrpc_server_utils::codecs;
use primitive_types::*;
use scillabackend::{ScillaBackend, ScillaBackendConfig};

use crate::precompiles::get_precompiles;
use crate::protos::Evm as EvmProto;
use protobuf::Message;

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

struct EvmServer {
    tracing: bool,
    backend_config: ScillaBackendConfig,
    gas_scaling_factor: u64,
}

impl EvmServer {
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
                )
                .boxed()
            }
            Err(e) => futures::future::err(e).boxed(),
        }
    }
}

#[allow(clippy::too_many_arguments)]
async fn run_evm_impl(
    address: H160,
    code: Vec<u8>,
    data: Vec<u8>,
    apparent_value: U256,
    gas_limit: u64,
    backend: ScillaBackend,
    tracing: bool,
    gas_scaling_factor: u64,
    estimate: bool,
) -> Result<String> {
    // We must spawn a separate blocking task (on a blocking thread), because by default a JSONRPC
    // method runs as a non-blocking thread under a tokio runtime, and creating a new runtime
    // cannot be done. And we'll need a new runtime that we can safely drop on a handled
    // panic. (Using the parent runtime and dropping on stack unwind will mess up the parent
    // runtime).
    tokio::task::spawn_blocking(move || {
        info!(
            "Executing EVM runtime: origin: {:?} address: {:?} gas: {:?} value: {:?} code: {:?} data: {:?}, extras: {:?}, estimate: {:?}",
            backend.origin, address, gas_limit, apparent_value, hex::encode(&code), hex::encode(&data),
            backend.extras, estimate);
        let code = Rc::new(code);
        let data = Rc::new(data);
        let config = evm::Config { estimate, ..evm::Config::london()};
        let context = evm::Context {
            address,
            caller: backend.origin,
            apparent_value,
        };
        let mut runtime = evm::Runtime::new(code, data, context, &config);
        // Scale the gas limit.
        let gas_limit = gas_limit * gas_scaling_factor;
        let metadata = StackSubstateMetadata::new(gas_limit, &config);
        let state = MemoryStackState::new(metadata, &backend);

        let precompiles = get_precompiles();

        let mut executor =
            evm::executor::stack::StackExecutor::new_with_precompiles(state, &config, &precompiles);

        let mut listener = LoggingEventListener{traces : Default::default()};

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
        let result = match result {
            Ok(exit_reason) => {
                let (state_apply, logs) = executor.into_state().deconstruct();
                info!(
                    "Return value: {:?}",
                    hex::encode(runtime.machine().return_value())
                );
                let mut result = EvmProto::EvmResult::new();
                result.set_exit_reason(exit_reason.into());
                result.set_return_value(runtime.machine().return_value().into());
                result.set_apply(state_apply
                        .into_iter()
                                 .map(|apply| {
                                     let mut result = EvmProto::Apply::new();
                                     match apply {
                                         Apply::Delete { address } => {
                                             let mut delete = EvmProto::Apply_Delete::new();
                                             delete.set_address(address.into());
                                             result.set_delete(delete);
                                         }
                                         Apply::Modify {
                                             address,
                                             basic,
                                             code,
                                             storage,
                                             reset_storage,
                                         } => {
                                             let mut modify = EvmProto::Apply_Modify::new();
                                             modify.set_address(address.into());
                                             modify.set_balance(basic.balance.into());
                                             modify.set_nonce(basic.nonce.into());
                                             if let Some(code) = code {
                                                 modify.set_code(code.into());
                                             }
                                             modify.set_reset_storage(reset_storage);
                                             modify.set_storage(storage.into_iter().map(Into::into).collect());
                                         }
                                     };
                                   result
                                 })
                                 .collect());
                result.set_trace(listener.traces.into_iter().map(Into::into).collect());
                result.set_logs(logs.into_iter().map(Into::into).collect());
                result.set_remaining_gas(remaining_gas);
                result
            },
            Err(panic) => {
                let panic_message = panic
                    .downcast::<String>()
                    .unwrap_or_else(|_| Box::new("unknown panic".to_string()));
                error!("EVM panicked: '{:?}'", panic_message);
                let mut result = EvmProto::EvmResult::new();
                let mut fatal = EvmProto::ExitReason_Fatal::new();
                fatal.set_kind(EvmProto::ExitReason_Fatal_Kind::OTHER);
                let mut exit_reason = EvmProto::ExitReason::new();
                exit_reason.set_fatal(fatal);
                result.set_exit_reason(exit_reason);
                result.set_trace(listener.traces.into_iter().map(Into::into).collect());
                result.set_remaining_gas(remaining_gas);
                result
            }
        };
        Ok(base64::encode(result.write_to_bytes().unwrap()))
    })
    .await
    .unwrap()
}

struct LoggingEventListener {
    pub traces: Vec<String>,
}

impl tracing::EventListener for LoggingEventListener {
    fn event(&mut self, event: tracing::Event) {
        self.traces.push(format!("{:?}", event));
    }
}

fn main() -> std::result::Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();

    match args.log4rs {
        Some(log_config) if !log_config.is_empty() => {
            log4rs::init_file(&log_config, Default::default())
                .with_context(|| format!("cannot open file {}", log_config))?;
        }
        _ => {
            let config_str = include_str!("../log4rs-local.yml");
            let config = serde_yaml::from_str(config_str).unwrap();
            log4rs::init_raw_config(config).unwrap();
        }
    }

    info!("Starting evm-ds");

    let evm_server = EvmServer {
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
    io.add_method("run", move |params| {
        let args = jsonrpc_core::Value::from(params);
        match args.get(0) {
            Some(arg) => evm_server
                .run(arg.to_string())
                .map(|result| result.map(jsonrpc_core::Value::from))
                .boxed(),
            None => {
                futures::future::err(jsonrpc_core::Error::invalid_params("No parameter")).boxed()
            }
        }
    });

    let shutdown_sender = std::sync::Mutex::new(shutdown_sender);
    // Have the "die" method send a signal to shut it down.
    // Mutex because the methods require all captured values to be Sync.
    // Set up a channel to shut down the servers
    io.add_method("die", move |_param| {
        shutdown_sender.lock().unwrap().send(()).unwrap();
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
