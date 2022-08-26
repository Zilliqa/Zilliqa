use futures::{SinkExt, StreamExt, TryStreamExt};
use jsonrpc_client_transports::transports::duplex::duplex;
use jsonrpc_core_client::{RpcChannel, RpcError};
use jsonrpc_server_utils::codecs::{Separator, StreamCodec};
use jsonrpc_server_utils::tokio;
use jsonrpc_server_utils::tokio_util::codec::Decoder as _;
use parity_tokio_ipc::Endpoint;
use std::path::Path;

/// Connect to a JSON-RPC IPC server.
pub async fn ipc_connect<P: AsRef<Path>, Client: From<RpcChannel>>(
    path: P,
) -> Result<Client, RpcError> {
    let connection = Endpoint::connect(path)
        .await
        .map_err(|e| RpcError::Other(Box::new(e)))?;
    let (sink, stream) = StreamCodec::new(Separator::default(), Separator::default())
        .framed(connection)
        .split();
    let sink = sink.sink_map_err(|e| RpcError::Other(Box::new(e)));
    let stream = stream.map_err(|e| log::error!("IPC stream error: {}", e));

    let (client, sender) = duplex(
        Box::pin(sink),
        Box::pin(
            stream
                .take_while(|x| futures::future::ready(x.is_ok()))
                .map(|x| x.expect("Stream is closed upon first error.")),
        ),
    );

    tokio::spawn(client);

    Ok(sender.into())
}
