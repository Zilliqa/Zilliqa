# Notes on Zilliqa persistence

> Warning: This document is non-normative and reverse engineered based on the code and data.

Nodes persist data in a `persistence` directory.
This directory contains a bunch of [LevelDB](https://github.com/google/leveldb) databases.

## LevelDB Databases

### Encoding

`u32`s are encoded as 0-padded ASCII strings.

`u64`s are encoded as ASCII strings.

`h160`s are encoded as hexadecimal strings.

`h256`s are encoded as either hexadecimal strings or big-endian byte arrays.

`MetaType` is encoded first as an integer then as an ASCII string.

### Sharded databases

Some database names in the table below end in `_*`.
These are sharded by time to ensure that no more than `NUM_EPOCHS_PER_PERSISTENT_DB` epochs worth of data exist in each instance.
The most recent instance is named without a `_n` suffix.

> Note: I haven't found where in the code we move the current instance to a suffixed one.

### Tries

Two databases store a trie - `contractTrie` and `state`.

> Note: The code makes it look like there are two key spaces per database (a 'main' and an 'aux'), but it turns out that the 'aux' store is always empty.

Therefore, all keys are `h256`s encoded as hexadecimal strings.
All values are byte strings.

These key value stores encode [Patricia Merkle Tries](https://ethereum.org/en/developers/docs/data-structures-and-encoding/patricia-merkle-trie/).

| Name | Keys | Values | Notes |
| ---- | ---- | ------ | ----------- |
| `blockLinks` | `u64` | [`ProtoBlockLink`] | Maps DS and VC block indices to information about each block. |
| `contractCode` | `h160` | `[u8]` | Stores the `code` associated with each account. |
| `contractCode_deprecated` | | | Deprecated |
| `contractCodeDeprecated` | | | Deprecated |
| `contractInitState2` | `h160` | `[u8]` | Stores the `initData` associated with each account. |
| `contractStateData` | | | Deprecated? |
| `contractStateData2` | `string` | `[u8]` | Stores state for each contract. Keys consist of the following, each separated by a 0x16 byte: <ul><li>a `h160` for the contract's address</li><li>an indicator (one of `"_fields_map_depth"`, `"_depth"`, `"_version"`, `"_type"`, `"_hasmap"`, `"_addr"` or a variable name from a contract)</li><li>a potentially empty list of index strings (in practice this list seems to have either 0 or 1 elements)</li></ul>The key indicator determines how to interpret values. |
| `contractStateIndex` | `h160` | [`ProtoStateIndex`] | Deprecated? |
| `contractTrie` | `h256` | `[u8]` | See [tries](#tries) |
| `dsBlocks` | `u64` | [`ProtoDsBlock`] | Maps DS block numbers to DS blocks. |
| `dsCommittee` | `u64` | `string; [PubKey; Peer]` | The value at key `0` will be a `u16` decimal string specifying the leader's ID. All remaining entries will have consecutive keys from `1` to `n`. There is an entry for each member of the DS commitee. Values are a concatenation of a [`Pubkey`] and a [`Peer`].
| `extSeedPubKeys` | `u32` | [`PubKey`] |
| `fallbackBlocks` | | | Deprecated? |
| `metadata` | [`MetaType`] | Depends on the type | `MetaType::LATESTACTIVEDSBLOCKNUM` maps to a `u64`, the rest look to be deprecated.
| `microBlockKeys` | `h256` | [`ProtoMicroBlockKey`] |
| `microBlocks_*` | [`ProtoMicroBlockKey`] | [`ProtoMicroBlock`] |
| `minerInfoDSComm` | `u64` | [`ProtoMinerInfoDsComm`] |
| `minerInfoShards` | `u64` | [`ProtoMinerInfoShards`] |
| `processedTxnTmp` | `h256` | [`ProtoTransactionReceipt`] |
| `shardStructure` | `u64` | `u32; ProtoShardingStructure` | For all `n`, values at `2n` will be a `u32` and values at `2n+1` will be a [`ProtoShardingStructure`].
| `state` | `h256` | [`ProtoAccountBase`] | See [tries](#tries) |
| `stateDelta` | `u64` | [`ProtoAccountStore`] |
| `stateRoot` | [`MetaType`] | Depends on the type |
| `tempState` | `h160` | [`ProtoAccountBase`] | The value encoding might be strange - We should check some example data to confirm.
| `txBlockHashToNum` | `h256` | `u64` |
| `txBlocks` | `u64` | [`ProtoTxBlock`] |
| `txBlocksAux` | `"MaxTxBlockNumber"` | `u64` |
| `txBodies_*` | `h256` | [`ProtoTransactionWithReceipt`] |
| `txBodiesTmp` | | | Deprecated? |
| `txEpochs` | `h256` | [`ProtoTxEpoch`] |
| `VCBlocks` | `h256` | [`ProtoVCBlock`] |

[`ProtoBlockLink`]: ../src/libMessage/ZilliqaMessage.proto#L19
[`ProtoStateIndex`]: ../src/libMessage/ZilliqaMessage.proto#L321
[`ProtoDsBlock`]: ../src/libMessage/ZilliqaMessage.proto#L55
[`ProtoMicroBlockKey`]: ../src/libMessage/ZilliqaMessage.proto#L280
[`ProtoMicroBlock`]: ../src/libMessage/ZilliqaMessage.proto#L119
[`ProtoMinerInfoDsComm`]: ../src/libMessage/ZilliqaMessage.proto#L252
[`ProtoMinerInfoShards`]: ../src/libMessage/ZilliqaMessage.proto#L264
[`ProtoTransactionReceipt`]: ../src/libMessage/ZilliqaMessage.proto#L419
[`ProtoShardingStructure`]: ../src/libMessage/ZilliqaMessage.proto#L145
[`ProtoAccountStore`]: ../src/libMessage/ZilliqaMessage.proto#L335
[`ProtoAccountBase`]: ../src/libMessage/ZilliqaMessage.proto#L296
[`ProtoTxBlock`]: ../src/libMessage/ZilliqaMessage.proto#L174
[`ProtoTransactionWithReceipt`]: ../src/libMessage/ZilliqaMessage.proto#L425
[`ProtoTxEpoch`]: ../src/libMessage/ZilliqaMessage.proto#L287
[`ProtoVCBlock`]: ../src/libMessage/ZilliqaMessage.proto#L204
[`MetaType`]: ../src/common/Constants.h#L85
[`Peer`]: ../src/libNetwork/Peer.cpp#L58
[`Pubkey`]: https://github.com/Zilliqa/schnorr/blob/master/src/libSchnorr/src/Schnorr_PubKey.cpp#L109
