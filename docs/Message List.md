# Message List

## Summary

Messages received by Zilliqa are forwarded for processing by its subclasses based on the Class byte.

| Class | Message Type     |
|:----- |:-----------------|
| 0x00  | PeerManager      |
| 0x01  | DirectoryService |
| 0x02  | Node             |

## PeerManager

| Ins   | Message   | Body                   | Action                          |
|:----- |:----------|:-----------------------|:--------------------------------|
| 0x00  | HELLO     | Public key + port      | Adds peer to store              |
| 0x01  | ADDPEER   | Public key + IP + port | Adds peer to store + says hello |
| 0x02  | PING      | Variable-length msg    | No action taken                 |
| 0x03  | PINGALL   | Variable-length msg    | Sends Ping message to all peers |
| 0x04  | BROADCAST | Variable-length msg    | Starts broadcast of message     |

## DirectoryService

| Ins  | Message              | Body                      | Action                           |
|:-----|:---------------------|:--------------------------|:---------------------------------|
| 0x00 | SETPRIMARY           | Primary node IP and port  | Set a node as DS, indicate the primary DS, and start POW1 processing phase |
| 0x01 | STARTPOW1            | Block num + difficulty + rand1 + rand2 + pubkey + IP and port of all DS nodes | Compute and multicast POW1 to all specified DS nodes |
| 0x02 | POW1SUBMISSION       | Submitter port + pubkey + nonce + hash + mixhash | Process POW1 submission |
| 0x03 | DSBLOCKCONSENSUS     | Consensus message         | Process consensus message, trigger POW2 processing phase when consensus DONE |
| 0x04 | DSBLOCK              | DSblock + rand1 + winner IP and port | Store DSblock and proceed to POW2 submission if node lost POW1 |
| 0x05 | POW2SUBMISSION       | Submitter port + pubkey + nonce + hash + mixhash | Process POW2 submission |
| 0x06 | SHARDINGCONSENSUS    | Consensus message         | Process consensus message, trigger microblock acceptance phase when consensus DONE |
| 0x07 | MICROBLOCKSUBMISSION | Shard ID + microblock     | Store microblock |
| 0x08 | FINALBLOCKCONSENSUS  | Consensus message         | Process consensus message, trigger finalblock sharing and new POW1 round when consensus DONE |
| 0x09 | FINALBLOCK           | Shard ID + finalblock + Tx body sharing list | Push finalblock into chain and do post-processing on transactions |

## Node

| Ins  | Message             | Body                      | Action                           |
|:-----|:--------------------|:--------------------------|:---------------------------------|
| 0x00 | STARTPOW2           | Block num + difficulty + rand1 + rand2 + pubkey + IP and port of all DS nodes | Compute and multicast POW1 to all specified DS nodes |
| 0x01 | SHARDING            | Shard ID + num shards + committee size + pubkey and IP and port of all shard nodes | Begin transaction submission |
| 0x02 | CREATETRANSACTION   | Submitter IP and port + from account address + to account address + amount + nonce | Check transaction and add to created list |
| 0x03 | SUBMITTRANSACTION   | Transaction body | Add transaction to received list |
| 0x04 | MICROBLOCKCONSENSUS | Consensus message | Process consensus message, trigger microblock multicast to all DS nodes when consensus DONE |
| 0x05 | FINALBLOCKAVAILABLE | Sharing mode + sharing configuration | Post-processing of all transactions and sharing of transaction bodies |
| 0x06 | FORWARDTRANSACTION  | Block num + transaction body | Add transaction to committed list and share body within committee |
