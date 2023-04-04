//! Implementation of EVM for Zilliqa

// #![deny(warnings)]
#![forbid(unsafe_code)]

mod continuations;
mod convert;
mod cps_executor;
mod evm_server;
mod evm_server_run;
mod ipc_connect;
mod precompiles;
mod pretty_printer;
mod protos;
mod scillabackend;

use serde::{Deserialize, Serialize};

use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use anyhow::Context;
use clap::Parser;

use evm_server::EvmServer;

use log::info;
use std::fmt::Debug;

use jsonrpc_core::IoHandler;
use jsonrpc_server_utils::codecs;
use scillabackend::ScillaBackendConfig;

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

    /// Log file (if not set, stderr is used).
    #[clap(short, long)]
    log4rs: Option<String>,

    /// How much EVM gas is one Scilla gas worth.
    #[clap(long, default_value = "1")]
    gas_scaling_factor: u64,

    /// Zil scaling factor.  How many Zils in one EVM visible Eth.
    #[clap(long, default_value = "1")]
    zil_scaling_factor: u64,

    /// Scilla root directory
    #[clap(long, default_value = "/scilla")]
    scilla_root_dir: String,

    /// Scilla libDir path
    #[clap(long, default_value = "/src/stdlib")]
    scilla_stdlib_dir: String,
}

#[derive(Debug, Serialize, Deserialize, Default)]
struct CallContext {
    #[serde(rename = "type")]
    pub call_type: String, // only 'call' (not create, delegate, static)
    pub from: String,
    pub to: String,
    pub value: String,
    pub gas: String,
    #[serde(rename = "gasUsed")]
    pub gas_used: String,
    pub input: String,
    pub output: String,

    calls: Vec<CallContext>,
}

impl CallContext {
    fn new() -> Self {
        CallContext {
            call_type: Default::default(),
            from: Default::default(),
            to: Default::default(),
            value: Default::default(),
            gas: "0x0".to_string(),
            gas_used: "0x0".to_string(),
            input: Default::default(),
            output: Default::default(),
            calls: Default::default(),
        }
    }
}

#[derive(Debug, Serialize, Deserialize, Default)]
struct StructLog {
    pub depth: usize,
    pub error: String,
    pub gas: u64, // not populated
    #[serde(rename = "gasCost")]
    pub gas_cost: u64, // not populated
    pub op: String,
    pub pc: usize,
    pub stack: Vec<String>,
    pub storage: Vec<String>, // not populated
}

// This implementation has a stack of call contexts each with reference to their calls - so a tree is
// Created in this way.
// Each new call gets added to the end of the stack and becomes the current context.
// On returning from a call, the end of the stack gets put into the item above's calls
#[derive(Debug, Serialize, Deserialize)]
struct LoggingEventListener {
    call_tracer: Vec<CallContext>,
    raw_tracer: StructLogTopLevel,
    enabled: bool,
}

#[derive(Debug, Serialize, Deserialize, Default)]
struct StructLogTopLevel {
    pub gas: u64,
    #[serde(rename = "returnValue")]
    pub return_value: String,
    #[serde(rename = "structLogs")]
    pub struct_logs: Vec<StructLog>,
}

impl LoggingEventListener {
    fn new(enabled: bool) -> Self {
        LoggingEventListener {
            call_tracer: Default::default(),
            raw_tracer: Default::default(),
            enabled,
        }
    }
}

impl evm::runtime::tracing::EventListener for LoggingEventListener {
    fn event(&mut self, event: evm::runtime::tracing::Event) {
        if !self.enabled {
            return;
        }

        let mut struct_log = StructLog {
            depth: self.call_tracer.len() - 1,
            ..Default::default()
        };

        match event {
            evm::runtime::tracing::Event::Step {
                context: _,
                opcode,
                position,
                stack,
                memory: _,
            } => {
                struct_log.op = format!("{opcode}");
                struct_log.pc = position.clone().unwrap_or(0);

                for sta in stack.data() {
                    struct_log.stack.push(format!("{sta:?}"));
                }
            }
            evm::runtime::tracing::Event::StepResult {
                result,
                return_value: _,
            } => {
                struct_log.op = "StepResult".to_string();
                struct_log.error = format!("{:?}", result.clone());
            }
            evm::runtime::tracing::Event::SLoad {
                address: _,
                index: _,
                value: _,
            } => {
                struct_log.op = "Sload".to_string();
            }
            evm::runtime::tracing::Event::SStore {
                address: _,
                index: _,
                value: _,
            } => {
                struct_log.op = "SStore".to_string();
            }
        }

        if self.raw_tracer.struct_logs.len() < 5 {
            self.raw_tracer.struct_logs.push(struct_log);
        }
    }
}

impl LoggingEventListener {
    #[allow(dead_code)]
    fn as_string(&self) -> String {
        serde_json::to_string(self).unwrap()
    }

    #[allow(dead_code)]
    fn as_string_pretty(&self) -> String {
        serde_json::to_string_pretty(self).unwrap()
    }

    fn finished_call(&mut self) {
        // The call has now completed - adjust the stack if neccessary
        if self.call_tracer.len() > 1 {
            let end = self.call_tracer.pop().unwrap();
            let new_end = self.call_tracer.last_mut().unwrap();
            new_end.calls.push(end);
        }
    }

    fn push_call(&mut self, context: CallContext) {
        // Now we have constructed our new call context, it gets added to the end of
        // the stack (if we want to do tracing)
        if self.enabled {
            self.call_tracer.push(context);
        }
    }
}

fn main() -> std::result::Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();

    match args.log4rs {
        Some(log_config) if !log_config.is_empty() => {
            log4rs::init_file(&log_config, Default::default())
                .with_context(|| format!("cannot open file {log_config}"))?;
        }
        _ => {
            let config_str = include_str!("../log4rs-local.yml");
            let config = serde_yaml::from_str(config_str).unwrap();
            log4rs::init_raw_config(config).unwrap();
        }
    }

    info!("Starting evm-ds");

    let backend_config = ScillaBackendConfig {
        path: PathBuf::from(args.node_socket),
        zil_scaling_factor: args.zil_scaling_factor,
    };

    let evm_server = EvmServer::new(backend_config, args.gas_scaling_factor);

    // Setup a channel to signal a shutdown.
    let (shutdown_sender, shutdown_receiver) = std::sync::mpsc::channel();

    let mut io = IoHandler::new();
    io.add_method("run", move |params| evm_server.run_json(params));

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
