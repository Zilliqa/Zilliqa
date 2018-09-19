# Zilliqa (codename _Durian_)

[![Build Status](https://travis-ci.com/Zilliqa/Zilliqa.svg?branch=master)](https://travis-ci.com/Zilliqa/Zilliqa)
[![codecov](https://codecov.io/gh/Zilliqa/Zilliqa/branch/master/graph/badge.svg)](https://codecov.io/gh/Zilliqa/Zilliqa)
[![Gitter chat](http://img.shields.io/badge/chat-on%20gitter-077a8f.svg)](https://gitter.im/Zilliqa/)

<p align="center">
  <img src="https://github.com/Zilliqa/Zilliqa/blob/master/img/zilliqa-logo-color.png" width="200" height="200">
</p>

## Overview

Zilliqa is a new blockchain platform capable of processing thousands of transactions per second with sharding built into it. With sharding, Zilliqa has the potential to match throughput benchmarks set by traditional payment methods (such as _VISA_ and _MasterCard_). More importantly, Zilliqa’s transaction throughput increases (roughly) linearly with its network size.

## _D24_ Testnet

* [Block Explorer](https://explorer.zilliqa.com/home)
* [Wallet](https://wallet.zilliqa.com/)  
  
## _D24_ Testnet with [Scilla](https://scilla-lang.org/) support

* [Interpreter IDE](https://ide.zilliqa.com/)  
* [Block Explorer](https://explorer-scilla.zilliqa.com)  
* [Wallet](https://wallet-scilla.zilliqa.com/)  

## Available features

The current release has the following features implemented:

* Proof of Work (PoW) for joining the network
* Network sharding
* Transaction sharding
* Directory Service
* Consensus for DS block (with sharding structure), Shard microblock and Final block 
* [EC-Schnorr signature](https://en.wikipedia.org/wiki/Schnorr_signature)
* Data layer and accounts store 
* Looking up nodes to allow new nodes to join 
* Persistent storage for transactions
* [Merkle Patricia tree](https://github.com/ethereum/wiki/wiki/Patricia-Tree)
* Transaction verification
* View change
* [Zilliqa Wallet](https://github.com/Zilliqa/Zilliqa-Wallet)
* [Smart contract design and implementation](https://scilla.readthedocs.io)
* [GPU (OpenCL and CUDA) support](https://github.com/Zilliqa/Zilliqa/wiki/GPU-mining) for PoW

In the coming months, we plan to have the following features:

* Gossip protocol for network message broadcasting
* Incentive structure
* Further unit and integration tests
* Enhancement of existing features
* More operating system support
* And much more ...

## Minimum system requirements

To run Zilliqa, we recommend the following minimum system requirements:

* x64 _Linux_ operating system such as _Ubuntu_
* Recent dual core processor
* 2 GB RAM

> Note: Presently we are in active development on Ubuntu 16.04. The support for
> building on other Ubuntu versions or other OSes is pending.

## Dependencies

* Ubuntu 16.04:

    ```bash
    sudo apt-get update
    sudo apt-get install git clang-format-5.0 clang-tidy-5.0 clang-5.0 libboost-system-dev \
        libboost-filesystem-dev libboost-test-dev libssl-dev libleveldb-dev libjsoncpp-dev \
        libsnappy-dev cmake libmicrohttpd-dev libjsonrpccpp-dev build-essential pkg-config \
        libevent-dev libminiupnpc-dev libprotobuf-dev protobuf-compiler
    ```

* macOS (experimental):

    ```bash
    brew install pkg-config jsoncpp leveldb libjson-rpc-cpp libevent miniupnpc
    ```

## Running Zilliqa locally

1. Build Zilliqa from the source.  

    ```
    ./build.sh
    ```

2. Run the local testnet script in `build` directory

    ```
    cd build && ./tests/Node/test_node_simple.sh
    ```

3. Logs of each node can be found at `./local_run`

4. To terminate Zilliqa,   
    ```
    pkill zilliqa
    ``` 

## Joining public testnet 

If you are interested in running a node on the Zilliqa testnet, please register [here](https://docs.google.com/forms/d/e/1FAIpQLSesn-MxThl4ocKCrjUYSPGc463E7-1I1O1v_GGccYbqfFr7zg/viewform).

## Further enquiries

* General question: [Slack](https://invite.zilliqa.com/)
* Development discussion: [Gitter](https://gitter.im/Zilliqa/)
* Bug report: [Github Issues](https://github.com/Zilliqa/zilliqa/issues)
* Security contact: `security` :globe_with_meridians: `zilliqa.com`

## Licence

You can view our [licence here](https://github.com/Zilliqa/zilliqa/blob/master/LICENSE).

