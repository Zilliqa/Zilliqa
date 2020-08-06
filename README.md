# Zilliqa (codename _Durian_)

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
* If you’d like to use the interface with Zilliqa nodes to transfer ZIL and deploy/call smart contracts, see here: https://apidocs.zilliqa.com/.
* If you’re interested in hacking on the Zilliqa code base, see the [Coding Guidelines](https://github.com/Zilliqa/Zilliqa/wiki/Coding-Guidelines).

> **NOTE**: The `master` branch is not for production as development is currently being worked constantly, please use the `tag` releases if you wish to work on the version of Zilliqa client that is running live on the Zilliqa blockchain. (Current live version `tag` release is `v6.3.0`)

## Zilliqa Mainnet

|          | URL(s) |
|:---------|:-------|
| **API URL** | `https://api.zilliqa.com/` |
| **Block Explorer** | [**Link**](https://viewblock.io/zilliqa) |

## Developer Testnet

|          | URL(s) |
|:---------|:-------|
| **API URL** | `https://dev-api.zilliqa.com/` |
| **Block Explorer** | [**Link**](https://dev-explorer.zilliqa.com) |
| **Faucet** | [**Link**](https://dev-wallet.zilliqa.com) |

## Available features

The current release has the following features implemented:

* Network sharding
* Transaction sharding
* Ethash Proof of Work (PoW) for joining the network
* GPU (OpenCL and CUDA) for PoW
* Gas rewards and pricer
* Coinbase rewards
* [EC-Schnorr signature](https://en.wikipedia.org/wiki/Schnorr_signature)
* pBFT Consensus mechanism
* Data layer and accounts store
* [Smart contract layer](https://scilla.readthedocs.io)
* State delta forwarding
* Lookup nodes and Seed nodes for receiving and dispatching transactions
* Persistent storage for transactions and state
* S3 storage retrieval from archival nodes
* View change mechanism
* Node recovery mechanism
* Protocol upgrade mechanism
* Gossip protocol for network message broadcasting

In the coming months, we plan to have the following features:

* Further unit and integration tests
* Enhancement of existing features
* More operating system support
* And much more...

## Minimum system requirements

To run Zilliqa, we recommend the minimum system requirements specified in our [Mining](https://github.com/Zilliqa/Zilliqa/wiki/Mining#hardware-requirement) page.

## Build Dependencies

The current supported version is **Ubuntu 16.04**.

Run the following to install the build dependencies:

```bash
sudo apt-get update
sudo apt-get install git libboost-system-dev libboost-filesystem-dev libboost-test-dev \
    libssl-dev libleveldb-dev libjsoncpp-dev libsnappy-dev cmake libmicrohttpd-dev \
    libjsonrpccpp-dev build-essential pkg-config libevent-dev libminiupnpc-dev \
    libcurl4-openssl-dev libboost-program-options-dev libboost-python-dev python3-dev \
    python3-setuptools python3-pip gawk
```

### Additional Requirements for Contributors

If you intend to contribute to the code base, please perform these additional steps:

1. Install `pyyaml`:

    ```bash
    pip install pyyaml
    ```

1. Create file `/etc/apt/sources.list.d/llvm-7.list` with the following contents ([reference](https://apt.llvm.org/)):

    ```
    deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-7 main
    deb-src http://apt.llvm.org/xenial/ llvm-toolchain-xenial-7 main
    ```

1. Run the following:

    ```bash
    curl https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
    sudo apt-get update
    sudo apt-get install clang-format-7 clang-tidy-7 -y
    ```

## Build from Source Code

Build Zilliqa from the source:

```shell
# download the lastest stable Zilliqa source code
$ git clone https://github.com/Zilliqa/Zilliqa.git
$ cd Zilliqa && git checkout tags/v6.3.0

# build Zilliqa binary
$ ./build.sh
```

If you want to build the development branch instead, do:

```shell
$ git checkout master
```

If you want to contribute by submitting code changes in a pull request, perform the build with `clang-format` and `clang-tidy` enabled by doing:

```shell
$ ./build.sh style
```

## Boot up a local testnet for development

1. Run the local testnet script in `build` directory:

    ```shell
    $ cd build && ./tests/Node/pre_run.sh && ./tests/Node/test_node_lookup.sh && ./tests/Node/test_node_simple.sh
    ```

2. Logs of each node can be found at `./local_run`.

3. To terminate Zilliqa:

    ```shell
    $ pkill zilliqa
    ```

## Further enquiries

|          | Link(s) |
|:---------|:-------|
| **Development discussion (discord)** | <a href="https://discord.gg/XMRE9tt" target="_blank"><img src="https://img.shields.io/discord/370992535725932544.svg" /></a> |
| **Bug report** | <a href="https://github.com/Zilliqa/zilliqa/issues" target="_blank"><img src="https://img.shields.io/github/issues/Zilliqa/zilliqa.svg" /></a> |
| **Security contact** | `security` :globe_with_meridians: `zilliqa.com` |
| **Security bug bounty** | https://bugcrowd.com/zilliqa |
