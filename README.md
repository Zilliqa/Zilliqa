# Zilliqa  

<p align="center">
    <img src="https://github.com/Zilliqa/Zilliqa/blob/master/img/zilliqa-logo-color.png" width="200" height="200">
</p>

<p align="center">
    <a href="https://travis-ci.com/Zilliqa/Zilliqa" target="_blank"><img src="https://travis-ci.com/Zilliqa/Zilliqa.svg?branch=master" /></a>
    <a href="https://codecov.io/gh/Zilliqa/Zilliqa" target="_blank"><img src="https://codecov.io/gh/Zilliqa/Zilliqa/branch/master/graph/badge.svg" /></a>
    <a href="https://github.com/Zilliqa/zilliqa/blob/master/LICENSE" target="_blank"><img src="https://img.shields.io/badge/license-GPL%20v3-green.svg" /></a>
</p>

## Overview

Zilliqa is a scalable smart contract platform that aims to tackle the congestion issue plaguing the blockchain industry. Zilliqa utilises a unique sharded architecture to achieve parallel processing of transactions while maintaining a large number of public nodes. Hence, Zilliqa is a blockchain capable of reaching high throughput and processing more complex computations while remaining decentralised and secure.

* If you’re interested in mining Zilliqa, see here: https://dev.zilliqa.com/docs/miners/mining-getting-started/.
* If you’d like to use the interface with Zilliqa nodes to transfer ZIL and deploy/call smart contracts, see here: https://dev.zilliqa.com/docs/apis/api-introduction.
* If you’re interested more about the core protocol, see the [developer portal](https://dev.zilliqa.com/docs/contributors/contribute-buildzil).
* If you’re interested in hacking on the Zilliqa code base, see the [Coding Guidelines](https://github.com/Zilliqa/Zilliqa/wiki/Coding-Guidelines).

> **NOTE**: The `master` branch is not for production as development is currently being worked constantly, please use the `tag` releases if you wish to work on the version of Zilliqa client that is running live on the Zilliqa blockchain.

## Zilliqa Mainnet

The current live version on the Zilliqa Mainnet is Zilliqa [v8.1.2](https://github.com/Zilliqa/Zilliqa/releases/tag/v8.1.2) and Scilla [v0.11.1](https://github.com/Zilliqa/scilla/releases/tag/v0.11.1).

|          | URL(s) |
|:---------|:-------|
| **API URL** | `https://api.zilliqa.com/` |
| **Block Explorer** | [**Viewblock**](https://viewblock.io/zilliqa) <br> [**DEVEX**](https://devex.zilliqa.com/) |

## Developer Testnet

The current live version on the Developer Testnet is Zilliqa [v8.5.0rc0](https://github.com/Zilliqa/Zilliqa/releases/tag/v8.5.0rc0) and Scilla [v0.13.1-alpha](https://github.com/Zilliqa/scilla/releases/tag/v0.13.1-alpha).

|          | URL(s) |
|:---------|:-------|
| **API URL** | `https://dev-api.zilliqa.com/` |
| **Block Explorer** | [**Viewblock**](https://dev-explorer.zilliqa.com) <br> [**DEVEX**](https://devex.zilliqa.com/?network=https%3A%2F%2Fdev-api.zilliqa.com) |
| **Faucet** | [**Link**](https://dev-wallet.zilliqa.com) |

## Zilliqa Improvement Proposal (ZIP)

The Zilliqa Improvement Proposals (ZIPs) are the core protocol standards for the Zilliqa platform.To view or contribute to ZIP, please visit https://github.com/Zilliqa/zip

## Available Features

The current release has the following features implemented:

* [Network sharding](https://dev.zilliqa.com/docs/basics/basics-zil-sharding#network-sharding)
* [Transaction sharding](https://dev.zilliqa.com/docs/basics/basics-zil-sharding#transaction-sharding)
* [Ethash Proof of Work (PoW) for joining the network](https://dev.zilliqa.com/docs/contributors/core-gossip)
* [GPU (OpenCL and CUDA) for PoW](https://dev.zilliqa.com/docs/contributors/core-pow#gpu-mine)
* [Block rewarding mechanism](https://dev.zilliqa.com/docs/basics/basics-zil-reward/)
* [Gas pricer](https://dev.zilliqa.com/docs/contributors/core-global-gas-price)
* [Coinbase rewards](https://dev.zilliqa.com/docs/contributors/core-coinbase)
* [EC-Schnorr signature](https://github.com/Zilliqa/schnorr)
* [pBFT Consensus mechanism](https://dev.zilliqa.com/docs/contributors/core-consensus)
* Data layer and accounts store
* [Smart contract layer](https://scilla.readthedocs.io)
* State delta forwarding
* Lookup nodes and Seed nodes for receiving and dispatching transactions
* Persistent storage for transactions and state
* S3 storage retrieval from archival nodes.
* [View change mechanism](https://dev.zilliqa.com/docs/contributors/core-view-change)
* Node recovery mechanism
* Protocol upgrade mechanism
* [Gossip protocol for network message broadcasting](https://dev.zilliqa.com/docs/contributors/core-gossip)
* [Seed Node Staking](https://dev.zilliqa.com/docs/staking/staking-overview)

In the coming months, we plan to have the following features:

* Further unit and integration tests
* Enhancement of existing features
* More operating system support
* And much more...

## Minimum System Requirements

To run Zilliqa, we recommend the minimum system requirements specified in our [Mining](https://dev.zilliqa.com/docs/miners/mining-zilclient#hardware-requirements) page.

## Build from Source Code

Starting with Zilliqa [v8.6.0](https://github.com/Zilliqa/Zilliqa/releases/tag/v8.6.0), the officially supported operating system is **Ubuntu 22.04**.

If you'd like to experiment with a different distro (including the previously supported Ubuntu 18.04), please make sure to install gcc >= 11.

Run the following to install the build dependencies:

```bash
sudo apt-get update
sudo apt-get install autoconf \
    build-essential \
    ccache \
    clang-format \
    clang-tidy \
    git \
    lcov \
    libcurl4-openssl-dev \
    libssl-dev \
    libtool \
    libxml2-utils \
    ninja-build \
    ocl-icd-opencl-dev \
    pkg-config \
    python3-dev \
    python3-pip \
    libgmp-dev \
    bison \
    gawk
git submodule update --init --recursive
```
Run the following to install latest version of cmake. CMake version >= 3.19 must be used:

```
wget https://github.com/Kitware/CMake/releases/download/v3.19.3/cmake-3.19.3-Linux-x86_64.sh
mkdir -p "${HOME}"/.local
bash ./cmake-3.19.3-Linux-x86_64.sh --skip-license --prefix="${HOME}"/.local/
export PATH=$HOME/.local/bin:$PATH
cmake --version
rm cmake-3.19.3-Linux-x86_64.sh
```

To install, clone vcpkg to a separate location (do not use brew on macos):

```shell
$ git clone https://github.com/Microsoft/vcpkg.git /path/to/vcpkg
$ cd /path/to/vcpkg && git checkout 2022.09.27 && ./bootstrap-vcpkg.sh
$ cd /path/to/zilliqa
$ export VCPKG_ROOT=/path/to/vcpkg
```
As part of building our source code, we patch websocketpp 0.8.2 to compile on C++20; please
see the license: https://github.com/zaphoyd/websocketpp/blob/master/COPYING.

Build Zilliqa from the source:

```shell
# build Zilliqa binary
$ ./build.sh
```

If you want to contribute by submitting code changes in a pull request perform the build with `clang-format` and `clang-tidy` enabled by doing:

```shell
$ ./build.sh style
```

## Build Scilla for Smart Contract Execution

The Zilliqa client works together with Scilla for executing smart contracts. Please refer to the [Scilla repository](https://github.com/Zilliqa/scilla) for build and installation instructions.

## Boot Up a Local Testnet for Development

1. Run the local testnet script in `build` directory:

    ```shell
    $ cd build && ./tests/Node/pre_run.sh && ./tests/Node/test_node_lookup.sh && ./tests/Node/test_node_simple.sh
    ```

2. Logs of each node can be found at `./local_run`

3. To terminate Zilliqa:

    ```shell
    $ pkill zilliqa
    ```

## Further Enquiries

|          | Link(s) |
|:---------|:-------|
| **Development discussion (discord)** | <a href="https://discord.gg/XMRE9tt" target="_blank"><img src="https://img.shields.io/discord/370992535725932544.svg" /></a> |
| **Bug report** | <a href="https://github.com/Zilliqa/zilliqa/issues" target="_blank"><img src="https://img.shields.io/github/issues/Zilliqa/zilliqa.svg" /></a> |
| **Security contact** | `security` :globe_with_meridians: `zilliqa.com` |
| **Security bug bounty** | <a href="https://hackerone.com/zilliqa" target="_blank">HackerOne bug bounty</a> |
