# Zilliqa (codename Chillicrab)
## Overview
Zilliqa is a new blockchain platform capable of processing thousands of transactions per second with sharding built into it. With sharding, Zilliqa has the potential to match throughput benchmarks set by traditional payment methods (such as VISA and MasterCard). Even more importantly, Zilliqa’s transaction throughput increases (roughly) linearly with its network size.

## Available Features
The current release has the following features implemented:
* Proof of Work 1 (PoW1) and 2 (PoW2) for joining the network
* Network sharding
* Consensus for DS block, Sharding structure, Shard Microblock and Final block 
* EC-Schnorr signature 
* Data layer and accounts store 
* Looking up nodes to allow new nodes to join 
* Persistent storage for transactions
* Merkle Patricia tree
* Transaction verification

In the coming months, we plan to have the following features:
* View change
* Gossip protocol for network message broadcasting
* Incentive structure
* Smart contract design and implementation
* GPU support for PoW
* Zilliqa Wallet 
* Further unit and integration tests
* Enhancement of existing features
* More operating system support
* And much more ...

## Minimum system requirements
To run Zilliqa, we recommend the following minimum system requirements:
* x64 Linux operating system such as Ubuntu
* Recent dual core processor
* 2 GB RAM
* A public IP address*

*In order to join the blockchain network, you will need a publicly accessible IP address. As Zilliqa does not support UPnP at the moment, you will need to do port forwarding if you are behind a NAT. For configuration of port forwarding, please refer to your router/gateway manual.  

## Dependencies
To compile and run the Zilliqa codebase, you will need the following dependencies to be installed on your machine:
* Boost 
* OpenSSL
* Jsoncpp
* Leveldb
* Cmake

For a debian-based system, you can use the following command to install the dependencies:  
`sudo apt-get install libboost-all-dev libssl-dev libleveldb-dev libjsoncpp-dev cmake`

# Running Zilliqa locally (using 10 shard nodes and DS node locally)  
Build Zilliqa from the source.  
` ./build.sh`

Run the local testnet script  
`./test/Node/test_node_simple.sh`  

Logs of each node can be found at code/local_run

## Running a Zilliqa Node on the public testnet 
Coming soon...

## For further enquiries
If you have issues running a node, do join our Zilliqa channel and ask us questions there! Our Slack invite link is https://invite.zilliqa.com/. You can also submit your issue at our github repository https://github.com/Zilliqa/zilliqa/issues 

## Licence 
You can view our licence at https://github.com/Zilliqa/zilliqa/blob/master/LICENSE

