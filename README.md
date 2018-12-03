# Zilliqa (codename _Durian_)

[![Build Status](https://travis-ci.com/Zilliqa/Zilliqa.svg?branch=master)](https://travis-ci.com/Zilliqa/Zilliqa)
[![codecov](https://codecov.io/gh/Zilliqa/Zilliqa/branch/master/graph/badge.svg)](https://codecov.io/gh/Zilliqa/Zilliqa)
[![Gitter chat](http://img.shields.io/badge/chat-on%20gitter-077a8f.svg)](https://gitter.im/Zilliqa/)

<p align="center">
  <img src="https://github.com/Zilliqa/Zilliqa/blob/master/img/zilliqa-logo-color.png" width="200" height="200">
</p>

## Overview

Zilliqa is a new blockchain platform capable of processing thousands of transactions per second with sharding built into it. With sharding, Zilliqa has the potential to match throughput benchmarks set by traditional payment methods (such as _VISA_ and _MasterCard_). More importantly, Zilliqa’s transaction throughput increases (roughly) linearly with its network size.

## _Mao Shan Wang_ public testnet

* [Block Explorer](https://explorer.zilliqa.com/)
* [API](https://api.zilliqa.com/)
* [Wallet](https://wallet.zilliqa.com/)
  
## _Mao Shan Wang_ small-scale testnet for developers

* [Block Explorer](https://explorer-scilla.zilliqa.com)
* [API](https://api-scilla.zilliqa.com/)

## Available features

The current release has the following features implemented:

* Single Ethash Proof of Work (PoW) for joining the network
* Network sharding
* Transaction sharding
* Directory Service Committee with Multiple-In, Multiple-Out (MIMO)
* pBFT Consensus for DS block (with sharding structure), Shard microblock, DS microblock, and Final block 
* [EC-Schnorr signature](https://en.wikipedia.org/wiki/Schnorr_signature)
* Data layer and accounts store
* Lookup nodes to allow new nodes to join and dispatch transactions to correct shards
* Persistent storage for transactions and state
* [Merkle Patricia tree](https://github.com/ethereum/wiki/wiki/Patricia-Tree)
* Transaction verification and receipt
* View change mechanism
* [Smart contract implementation](https://scilla.readthedocs.io)
* [GPU (OpenCL and CUDA) support](https://github.com/Zilliqa/Zilliqa/wiki/Mining) for PoW
* State delta forwarding
* Gossip protocol for network message broadcasting
* Protocol upgrade mechanism
* Node recovery mechanism
* Archival nodes
* Gas rewards and pricer
* Coinbase rewards

In the coming months, we plan to have the following features:

* Mining curve structure
* Further unit and integration tests
* Enhancement of existing features
* More operating system support
* And much more ...

## Minimum system requirements

To run Zilliqa, we recommend the following minimum system requirements:

* x64 _Linux_ operating system such as _Ubuntu_
* Intel i5 processor or later
* 2 GB RAM or higher

> Note: Presently, we are in active development on Ubuntu 16.04. We also support macOS.
> Support for building on other Debian-based distributions are pending.

## Dependencies

* Ubuntu 16.04:

    ```bash
    sudo apt-get update
    sudo apt-get install git libboost-system-dev libboost-filesystem-dev libboost-test-dev \
        libssl-dev libleveldb-dev libjsoncpp-dev libsnappy-dev cmake libmicrohttpd-dev \
        libjsonrpccpp-dev build-essential pkg-config libevent-dev libminiupnpc-dev \
        libprotobuf-dev protobuf-compiler libcurl4-openssl-dev
    ```

* macOS:

    ```bash
    brew install boost pkg-config jsoncpp leveldb libjson-rpc-cpp libevent miniupnpc protobuf
    ```

## Running Zilliqa locally

1. Build Zilliqa from the source.  

    ```
    ./build.sh
    ```

2. Run the local testnet script in `build` directory

    ```
   cd build && ./tests/Node/pre_run.sh && ./tests/Node/test_node_lookup.sh && ./tests/Node/test_node_simple.sh
    ```

3. Logs of each node can be found at `./local_run`

4. To terminate Zilliqa,   
    ```
    pkill zilliqa
    ``` 

## Joining the _Mao Shan Wang_ public testnet

Please visit the [Mining wiki](https://github.com/Zilliqa/Zilliqa/wiki/Mining) to find out more.


## Further enquiries

* General question: [Slack](https://invite.zilliqa.com/)
* Development discussion: [Gitter](https://gitter.im/Zilliqa/)
* Bug report: [Github Issues](https://github.com/Zilliqa/zilliqa/issues)
* Security contact: `security` :globe_with_meridians: `zilliqa.com`

## Licence

You can view our [licence here](https://github.com/Zilliqa/zilliqa/blob/master/LICENSE).

