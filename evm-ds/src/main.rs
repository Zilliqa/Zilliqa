//! Implementation of EVM for Zilliqa

// #![deny(warnings)]
#![forbid(unsafe_code)]

mod convert;
mod ipc_connect;
mod precompiles;
mod protos;
mod scillabackend;

//use std::default::default;
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use std::panic::{self, AssertUnwindSafe};
use std::path::PathBuf;
use std::sync::{Arc, Mutex};
use std::rc::{Rc, Weak};

use serde::ser::{SerializeStruct, Serializer};
//use serde_json::Serializer::;
use serde::{Deserialize, Serialize};

use anyhow::Context;
use clap::Parser;
use evm::{
    backend::Apply,
    executor::stack::{MemoryStackState, StackSubstateMetadata},
    tracing,
};
use futures::FutureExt;

use log::{debug, error, info};
use std::fmt::Debug;

use jsonrpc_core::{BoxFuture, Error, IoHandler, Result};
use jsonrpc_server_utils::codecs;
use primitive_types::*;
use scillabackend::{ScillaBackend, ScillaBackendConfig};

use crate::precompiles::get_precompiles;
use crate::protos::Evm as EvmProto;
use protobuf::{Chars, Message};

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
                    args.get_context().to_string(),
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
    evm_context: String,
) -> Result<String> {
    // We must spawn a separate blocking task (on a blocking thread), because by default a JSONRPC
    // method runs as a non-blocking thread under a tokio runtime, and creating a new runtime
    // cannot be done. And we'll need a new runtime that we can safely drop on a handled
    // panic. (Using the parent runtime and dropping on stack unwind will mess up the parent runtime).
    tokio::task::spawn_blocking(move || {
        println!(
            "Running EVM: origin: {:?} address: {:?} gas: {:?} value: {:?} code: {:?} data: {:?}, extras: {:?}, estimate: {:?}",
            backend.origin, address, gas_limit, apparent_value, hex::encode(&code), hex::encode(&data),
            backend.extras, estimate);

        //let mut listener = LoggingEventListener{traces : Default::default()};
        let mut listener = LoggingEventListener::new();
        listener.call_stack[0].call_type = "CALL".to_string();
        listener.call_stack[0].from = format!("{:?}", backend.origin);
        listener.call_stack[0].to = format!("{:?}", address);
        listener.call_stack[0].value = apparent_value.to_string();
        listener.call_stack[0].input = hex::encode(&data);

        let code = Rc::new(code);
        let data = Rc::new(data);
        // TODO: handle call_l64_after_gas problem: https://zilliqa-jira.atlassian.net/browse/ZIL-5012
        let config = evm::Config { estimate, call_l64_after_gas: false, ..evm::Config::london()};
        let context = evm::Context {
            address,
            caller: backend.origin,
            apparent_value,
        };
        let mut runtime = evm::Runtime::new(code.clone(), data.clone(), context, &config);
        // Scale the gas limit.
        let gas_limit = gas_limit * gas_scaling_factor;
        let metadata = StackSubstateMetadata::new(gas_limit, &config);
        let state = MemoryStackState::new(metadata, &backend);

        let precompiles = get_precompiles();

        let mut executor =
            evm::executor::stack::StackExecutor::new_with_precompiles(state, &config, &precompiles);

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
                match exit_reason {
                    evm::ExitReason::Succeed(_) => {}
                    _ => {
                        debug!("Machine: position: {:?}, memory: {:?}, stack: {:?}",
                               runtime.machine().position(),
                               &runtime.machine().memory().data().iter().take(128).collect::<Vec<_>>(),
                               &runtime.machine().stack().data().iter().take(128).collect::<Vec<_>>());
                    }
                }
                let mut result = EvmProto::EvmResult::new();
                result.set_exit_reason(exit_reason.into());
                result.set_return_value(runtime.machine().return_value().into());
                let (state_apply, logs) = executor.into_state().deconstruct();
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
                                             debug!("Modify: {:?} {:?}", address, basic);
                                             let mut modify = EvmProto::Apply_Modify::new();
                                             modify.set_address(address.into());
                                             modify.set_balance(backend.scale_eth_to_zil(basic.balance).into());
                                             modify.set_nonce(basic.nonce.into());
                                             if let Some(code) = code {
                                                 modify.set_code(code.into());
                                             }
                                             modify.set_reset_storage(reset_storage);
                                             let storage_proto = storage.into_iter().map(
                                                 |(k, v)| backend.encode_storage(k, v).into()).collect();
                                             modify.set_storage(storage_proto);
                                             result.set_modify(modify);
                                         }
                                     };
                                   result
                                 })
                                 .collect());


                // Collect the listener infos
                listener.call_stack[0].output = hex::encode(runtime.machine().return_value());
                let serialized_listener = serde_json::to_string_pretty(&listener.call_stack).unwrap();

                println!("serialized listener: {:?}", serialized_listener);

                //let collected: Vec<Chars> = serialized_listener.as_bytes().to_vec();

                result.set_trace(vec![serialized_listener].into_iter().map(Into::into).collect());
                result.set_logs(logs.into_iter().map(Into::into).collect());
                result.set_remaining_gas(remaining_gas);
                info!(
                    "EVM execution summary: context: {:?}, origin: {:?} address: {:?} gas: {:?} value: {:?} code: {:?} data: {:?}, extras: {:?}, estimate: {:?}, result: {:?}\n", evm_context,
                    backend.origin, address, gas_limit, apparent_value, hex::encode(code.as_ref()), hex::encode(data.as_ref()),
                    backend.extras, estimate, result);
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
                //result.set_trace(listener.traces.into_iter().map(Into::into).collect());
                result.set_remaining_gas(remaining_gas);
                result
            }
        };
        Ok(base64::encode(result.write_to_bytes().unwrap()))
    })
    .await
    .unwrap()
}

#[derive(Debug,Serialize)]
struct CallContext {
    #[serde(rename = "type")]
    pub call_type : String,
    pub from : String,
    pub to : String,
    pub value : String,
    pub gas : String,
    pub gasUsed : String,
    pub input : String,
    pub output : String,

    calls: Vec<CallContext>,
}

impl CallContext {
    fn new() -> Self {
        CallContext{
            call_type : Default::default(),
            from : Default::default(),
            to : Default::default(),
            value : Default::default(),
            gas : "0x0".to_string(),
            gasUsed : "0x0".to_string(),
            input : Default::default(),
            output : Default::default(),
            calls: Default::default(),
        }
    }
}

// This implementation has a stack of call contexts each with reference to their calls - so a tree is
// Created in this way.
// Each new call gets added to the end of the stack and becomes the current context.
// On returning from a call, the end of the stack gets put into the item above's calls
struct LoggingEventListener {
    pub call_stack: Vec<CallContext>,
}

impl LoggingEventListener {

    fn new() -> Self {
        LoggingEventListener {
            call_stack: vec![CallContext::new()],
        }
    }
}
//// This is what #[derive(Serialize)] would generate.
//// We need a custom impl becasue one of the fields is 'type' which is a reserved word
//impl Serialize for CallContext {
//    fn serialize<S>(&self, serializer: S) -> serde::ser::Result<S::Ok, S::Error>
//    where
//        S: Serializer,
//    {
//        let mut s = serializer.serialize_struct("CallContext", 3)?;
//        s.serialize_field("type", &self.call_type)?;
//        s.serialize_field("from", &self.from)?;
//        s.serialize_field("to", &self.to)?;
//        s.serialize_field("value", &self.value)?;
//        s.serialize_field("gas", &self.gas)?;
//        s.serialize_field("gasUsed", &self.gasUsed)?;
//        s.serialize_field("input", &self.input)?;
//        s.serialize_field("output", &self.output)?;
//        s.serialize_field("calls", &self.calls)?;
//        s.end()
//    }
//}

// "type": "CALL",
//    "from": "0x5067c042e35881843f2b31dfc2db1f4f272ef48c",
//    "to": "0x3ee18b2214aff97000d974cf647e7c347e8fa585",
//    "value": "0x0",
//    "gas": "0x17459",
//    "gasUsed": "0x166cb",
//    "input": "0x0f5287b0000000000000000000000000a0b86991c6218b36c1d19d4a2e9eb0ce3606eb48000000000000000000000000000000000000000000000000000000001debea42000000000000000000000000000000000000000000000000000000000000000167c46aa713cfe47608dd1c16f8a0325208df084c3cbebf9f366ad0eafc2653e400000000000000000000000000000000000000000000000000000000001e8542000000000000000000000000000000000000000000000000000000006eca0000",
//    "output": "0x000000000000000000000000000000000000000000000000000000000001371e",

// "type": "CALL",
//            "from": "0x3ee18b2214aff97000d974cf647e7c347e8fa585",
//            "to": "0x98f3c9e6e3face36baad05fe09d375ef1464288b",
//            "value": "0x0",
//            "gas": "0x4f9f",
//            "gasUsed": "0x46c6",
//            "input": "0xb19a437e000000000000000000000000000000000000000000000000000000006eca00000000000000000000000000000000000000000000000000000000000000000060000000000000000000000000000000000000000000000000000000000000000f000000000000000000000000000000000000000000000000000000000000008501000000000000000000000000000000000000000000000000000000001debea42000000000000000000000000a0b86991c6218b36c1d19d4a2e9eb0ce3606eb48000267c46aa713cfe47608dd1c16f8a0325208df084c3cbebf9f366ad0eafc2653e4000100000000000000000000000000000000000000000000000000000000001e8542000000000000000000000000000000000000000000000000000000",
//            "output": "0x000000000000000000000000000000000000000000000000000000000001371e",
//            "calls": [
//              {
//                "type": "DELEGATECALL",
//                "from": "0x98f3c9e6e3face36baad05fe09d375ef1464288b",
//                "to": "0x8c0041566e0bc27efe285a9e98d0b4217a46809c",
//                "gas": "0x3b88",
//                "gasUsed": "0x3377",
//                "input": "0xb19a437e000000000000000000000000000000000000000000000000000000006eca00000000000000000000000000000000000000000000000000000000000000000060000000000000000000000000000000000000000000000000000000000000000f000000000000000000000000000000000000000000000000000000000000008501000000000000000000000000000000000000000000000000000000001debea42000000000000000000000000a0b86991c6218b36c1d19d4a2e9eb0ce3606eb48000267c46aa713cfe47608dd1c16f8a0325208df084c3cbebf9f366ad0eafc2653e4000100000000000000000000000000000000000000000000000000000000001e8542000000000000000000000000000000000000000000000000000000",
//                "output": "0x000000000000000000000000000000000000000000000000000000000001371e"
//              }

impl tracing::EventListener for LoggingEventListener {
    fn event(&mut self, event: tracing::Event) {

        println!("recvd event: {:?}", event);

        if self.call_stack.is_empty() {
            error!("call stack empty in listener!!! ");
        }

        match event {
            tracing::Event::Call{code_address, transfer, input, target_gas, is_static, context} => {
                // When there is a call, add a call context to bottom of stack
                let mut call_to_push = CallContext::new();
                let mut end_of_stack = self.call_stack.last().unwrap();

                call_to_push.call_type = "CALL".to_string();
                call_to_push.from = end_of_stack.to.clone();
                call_to_push.to = code_address.to_string();
                call_to_push.gas = format!("{:x}", target_gas.unwrap_or(0));
                call_to_push.gasUsed = "0x0".to_string(); // todo
                call_to_push.input = hex::encode(input);
                call_to_push.output = "0x0".to_string();
                if let Some(trans) = transfer {
                    call_to_push.value = trans.value.to_string();
                }

                // Now we have constructed our new call context, it gets added to the end of
                // the stack
                //end_of_stack.calls.push(call_to_push);
                self.call_stack.push(call_to_push);
            },
            tracing::Event::Create{..} => {},
            tracing::Event::Suicide{..} => {},
            tracing::Event::Exit{..} => {
                // The call has now completed - adjust the stack if neccessary
                if self.call_stack.len() > 1 {
                    let end = self.call_stack.pop().unwrap();
                    let new_end = self.call_stack.last_mut().unwrap();
                    new_end.calls.push(end);
                }
            },
            tracing::Event::TransactCall{..} => {},
            tracing::Event::TransactCreate{..} => {},
            tracing::Event::TransactCreate2{..} => {},
            tracing::Event::PrecompileSubcall{..} => {},
        }
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
        if let Some(arg) = args.get(0) {
            if let Some(arg_str) = arg.as_str() {
                evm_server
                    .run(arg_str.to_string())
                    .map(|result| result.map(jsonrpc_core::Value::from))
                    .boxed()
            } else {
                futures::future::err(jsonrpc_core::Error::invalid_params("Invalid parameter"))
                    .boxed()
            }
        } else {
            futures::future::err(jsonrpc_core::Error::invalid_params("No parameter")).boxed()
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
