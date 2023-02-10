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

use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use anyhow::Context;
use clap::Parser;
use evm::tracing;

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

    let backend_config = ScillaBackendConfig {
        path: PathBuf::from(args.node_socket),
        zil_scaling_factor: args.zil_scaling_factor,
    };

    let evm_server = EvmServer::new(args.tracing, backend_config, args.gas_scaling_factor);

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
