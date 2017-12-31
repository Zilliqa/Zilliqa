# Data Storage

This page summarizes the different kinds of information stored by a Zilliqa instance related to internal state and the blockchain.

## DSBlock

| Field                  | Impl Size (bytes)     |
|:-----------------------|:----------------------|
| **Fields described in Whitepaper** |<br/>      |
| version                | _to be implemented_   |
| previous hash          | 32                    |
| pubkey                 | 33                    |
| difficulty             |  1                    |
| number                 | 32                    |
| timestamp              | 32                    |
| mixHash                | _to be implemented_   |
| nonce                  | 32                    |
| signature              | 64                    |
| bitmap                 | _to be implemented_   |
| **Additional fields**  | <br/>                 |
| leader pubkey          | 33                    |
| **TOTAL SIZE**         | 259                   |

## Transaction

| Field                  | Impl Size (bytes)     |
|:-----------------------|:----------------------|
| **Fields described in Whitepaper** |<br/>      |
| version                | _to be implemented_   |
| nonce                  | 32                    |
| to                     | 20                    |
| amount                 | 32                    |
| gas price              | _to be implemented_   |
| gas limit              | _to be implemented_   |
| code                   | _to be implemented_   |
| data                   | _to be implemented_   |
| pubkey                 | _to be implemented_   |
| signature              | 64                    |
| transaction ID         | 32                    |
| **Additional fields**  | <br/>                 |
| from address           | 20                    |
| **TOTAL SIZE**         | 200                   |

## TxBlock

| Field                  | Impl Size (bytes)     |
|:-----------------------|:----------------------|
| **Fields described in Whitepaper** | <br/>     |
| type                   |  1                    |
| version                |  4                    |
| previous hash          | 32                    |
| gas limit              | 32                    |
| gas used               | 32                    |
| number                 | 32                    |
| timestamp              | 32                    |
| state root             | _to be implemented_   |
| transaction root       | 32                    |
| tx hashes              | 32 * tx count         |
| pubkey                 | 33                    |
| pubkey micro blocks    | _to be implemented_   |
| parent block hash      | _to be implemented_   |
| parent ds hash         | 32                    |
| parent ds block number | 32                    |
| tx count               |  4                    |
| tx list                | _stored separately_   |
| signature              | 64                    |
| bitmap                 | _to be implemented_   |
| **TOTAL SIZE**         | 362 + (32 * tx count) |

## Storage Estimates

| Txns per block | TxBlock + txns |
|:---------------|:---------------|
| 500            |  114 kB        |
| 1000           |  227 kB        |
| 2000           |  453 kB        |
| 3000           |  680 kB        |
| 4000           |  907 kB        |
| 5000           | 1.11 MB        |
| 10000          | 2.21 MB        |