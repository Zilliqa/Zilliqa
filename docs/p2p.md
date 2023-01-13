# Peer-to-Peer Protocol

Nodes send messages to each other.

## Messages

### `LookupGetDsInfoFromSeed`

Request for DS committee information.
The response is sent via a `LookupSetDsInfoFromSeed` message.
Setting `initialDS = true` will result in the message being silently dropped.

Sent from a normal node to a lookup node.

### `LookupSetDsInfoFromSeed`

Specifies the current DS committee and DS leader.

Sent from a DS leader to all lookup nodes in response to a `SetPrimary` message.
Sent from a lookup node to a normal node in response to a `LookupGetDsInfoFromSeed` message.

### `LookupGetDsBlockFromSeed`

Request for a list of DS blocks within a specified range.
The response is sent via a `LookupSetDsBlockFromSeed` message.
If `highblocknum = 0`, the range will include the latest block.
Only the first `FETCH_DS_BLOCK_LIMIT` blocks in the specified range will be returned.
If `includeminerinfo = true`, a separate `LookupSetMinerInfoFromSeed` response message will also be sent.

Sent to a lookup node.

### `LookupSetDsBlockFromSeed`

Specifies a list of DS blocks.

Sent from a lookup node in response to a `LookupGetDsBlockFromSeed` message.

### `LookupGetTxBlockFromSeed`

Request for a list of TX blocks within a specified range.
The response is sent via a `LookupSetTxBlockFromSeed` message.
If `highblocknum = 0`, the range will include the latest block.

Sent to a lookup node.

### `LookupSetTxBlockFromSeed`

Specifies a list of TX blocks.

Sent from a lookup node in response to a `LookupGetTxBlockFromSeed` message.

### `LookupSetLookupOffline`

Specifies that the sender is offline.

Sent from a lookup node to all other lookup nodes.
This is sent on startup from non-archival lookup nodes.

### `LookupSetLookupOnline`

Specifies that the sender is online.



### `LookupGetOfflineLookups`

### `LookupSetOfflineLookups`

### `LookupGetShardsFromSeed`

### `LookupSetShardsFromSeed`

### `LookupGetMicroBlockFromLookup`

### `LookupSetMicroBlockFromLookup`

### `LookupGetTxnsFromLookup`

### `LookupSetTxnsFromLookup`

### `LookupGetDirectoryBlocksFromSeed`

### `LookupSetDirectoryBlocksFromSeed`

### `LookupGetStateDeltaFromSeed`

### `LookupSetStateDeltaFromSeed`

### `LookupGetStateDeltasFromSeed`

### `LookupSetStateDeltasFromSeed`

### `LookupGetDsTxBlockFromSeed`

### `LookupForwardTxnsFromSeed`

### `NodeGetGuardNodeNetworkInfoUpdate`

### `LookupGetCosigsRewardsFromSeed`

### `LookupSetMinerInfoFromSeed`

### `LookupGetDsBlockFromL2l`

### `LookupGetVcFinalBlockFromL2l`

### `LookupGetMBnForwardTxnFromL2l`

### `LookupGetMicroBlockFromL2l`

### `LookupGetTxnsFromL2l`

### `NodeForwardTxnBlock`

### `NodeGetVersion`

### `NodeSetVersion`

### `NodeVcFinalBlock`

### `SetPrimary` and friends??? (see `DirectoryService::Execute`)
