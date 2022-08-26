# EVM-DS is an EVM implementation for Zilliqa

EVM-DS is designed to run on Zilliqa Directory Service nodes.

## Building it

Install the Rust toolchain, then do:

```
cargo build --release
```

As in all Rust projects, the binary will be found in `target/release/evm-ds`, (unless the cargo configuration is changed locally, then according to the configuration).

## Running it


Running EVM-DS is similar to the Scilla interpreter. The Zilliqa node should run `evm-ds` as a subprocess.

Arguments:

  * `--socket`: Path of the EVM server Unix domain socket. The `evm-ds` binary will be the server listening on this socket and accepting EVM code execution requests on it. Default is `/tmp/evm-server.sock`.
  
  * `--node_socket`: Path of the Node Unix domain socket. The `evm-ds` binary will be the client requesting account and state data from the Zilliqa node. Default is `/tmp/zilliqa.sock`.

  * `--http_port`: an HTTP port serving the same purpose as the `--socket` above. It is needed only for debugging of `evm-ds`, as there are way more tools for HTTP JSON-RPC, than for Unix sockets.
  
  * `--tracing`: if true, additional trace logging will be enabled.
  

## JSON-RPC methods

  * `EvmResult run(string address, string caller, string code, string data, string apparent_value)` - run execution of `code` with calldata `data`, as a contract at address `address`, on behalf of account `caller`. `apparent_value` is the message funds in WEI.

Returns: a dictionary of the form:
```
{
  "Ok": {
    "exit_reason": { "Succeed": "Stopped" },
    "return_value": "48656c6c6f20776f726c6421",      // some string hex value.
    "apply": [ {"A": "modify", "address": "<address>", "balance": 12345, "nonce": 2,
                "code": "608060405234801561001057600080fd5b50600436106100415", // new EVM code for address
                "storage": [["<key in hex>", "<value in hex>"], ["<key in hex>", "<value in hex>"] ... ],
                "reset_storage": false,  // whether to wipe the account storage before appying changes.
                },
               ...
               {"A": "delete", "address": "<address of account to delete">},
               ...
               ],

     "logs": [ { ... log entry ...}, { ... log entry ... }]    // will be specified.
}
```

or
```
{
   "Err": error_object // - can be just printed to the log message
}
```


