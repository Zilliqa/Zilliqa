# Zilliqa (codename Durian)
## Overview
Zilliqa is a new blockchain platform capable of processing thousands of transactions per second with sharding built into it. With sharding, Zilliqa has the potential to match throughput benchmarks set by traditional payment methods (such as VISA and MasterCard). Even more importantly, Zilliqa’s transaction throughput increases (roughly) linearly with its network size.

## Available Features
The current release has the following features implemented:
* Proof of Work 1 (PoW1) and 2 (PoW2) for joining the network
* Network sharding
* Directory Service
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
1. Build Zilliqa from the source.  
` ./build.sh`

2. Run the local testnet script  
`./test/Node/test_node_simple.sh`  

3. Logs of each node can be found at code/local_run

4. To terminate zilliqa,   
`pkill zilliqa` 

## Running a Zilliqa Node on the public testnet 
Coming soon...

## For further enquiries
If you have issues running a node, do join our Zilliqa channel and ask us questions there! Our Slack invite link is https://invite.zilliqa.com/. You can also submit your issue at our github repository https://github.com/Zilliqa/zilliqa/issues 

## Licence 
You can view our licence at https://github.com/Zilliqa/zilliqa/blob/master/LICENSE

