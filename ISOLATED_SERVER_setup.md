# Zilliqa Isolated Server Instructions

Zilliqa Isolated Server is a test server for dApp developers to quickly test their applications. The isolated server is different from [kaya-rpc](https://github.com/Zilliqa/kaya) as it uses Zilliqa's codebase, and is therefore more regularly updated than Kaya-RPC. 
Transactions are validated immediately, hence improving the productivity for dApp developers.

**This was tested on AWS EC2 t2.micro Ubuntu 16.04.6 LTS with 4GB swapfile**

## Install Scilla (Prerequisite)
Instructions below is only for Ubuntu 16.04 LTS

Please follow this link for the latest installation instructions for other OS. https://github.com/Zilliqa/scilla/blob/master/INSTALL.md#ubuntu

```
cd /home/ubuntu
git clone https://github.com/Zilliqa/scilla.git
sudo add-apt-repository ppa:tah83/secp256k1 -y //Ignore this line if you are building on Ubuntu 18
sudo add-apt-repository -y ppa:avsm/ppa
sudo apt-get update
sudo apt-get install -y curl build-essential m4 ocaml opam pkg-config zlib1g-dev libgmp-dev libffi-dev libssl-dev libboost-system-dev libsecp256k1-dev libpcre3-dev

opam init --compiler=4.06.1 --yes
eval $(opam env)
cd scilla
opam install ./scilla.opam --deps-only --with-test
make clean; make
```

## Steps to Build & Run Node
1. `git clone https://github.com/Zilliqa/Zilliqa.git`
2. cd `Zilliqa`
3. Edit the `./constants.xml` with the following attributes. Please take note of the directory where you installed scilla.
```
<LOOKUP_NODE_MODE>true</LOOKUP_NODE_MODE>
<ENABLE_SC>true</ENABLE_SC>
<SCILLA_ROOT>/home/ubuntu/scilla</SCILLA_ROOT>
<ENABLE_SCILLA_MULTI_VERSION>false</ENABLE_SCILLA_MULTI_VERSION>
```
4. Create swapfile
```
// Recommended to have at least 4GB of free memory
// You can skip if you have sufficient memory to build Zilliqa

swapon --show
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
sudo cp /etc/fstab /etc/fstab.bak
sudo vi /etc/fstab

// add the following line into /etc/fstab
/swapfile none swap sw 0 0

// verify
sudo swapon --show
```
5. Run `./build.sh` Build failures are mainly due to lack of memory. You can assign swap space if your hardware is lacking memory. (tested on AWS t2.micro with 4gb swap space)
6. Create the file that contains the bootstrap TEST ACCOUNTS `vi isolated-server-accounts.json` with the following json
```
{
    "7bb3b0e8a59f3f61d9bff038f4aeb42cae2ecce8": {
        "privateKey": "db11cfa086b92497c8ed5a4cc6edb3a5bfe3a640c43ffb9fc6aa0873c56f2ee3",
        "amount": "1000000000000000000",
        "nonce": 0
    },
    "d90f2e538ce0df89c8273cad3b63ec44a3c4ed82": {
        "privateKey": "e53d1c3edaffc7a7bab5418eb836cf75819a82872b4a1a0f1c7fcf5c3e020b89",
        "amount": "1000000000000000000",
        "nonce": 0
    },
    "381f4008505e940ad7681ec3468a719060caf796": {
        "privateKey": "d96e9eb5b782a80ea153c937fa83e5948485fbfc8b7e7c069d7b914dbc350aba",
        "amount": "1000000000000000000",
        "nonce": 0
    },
    "b028055ea3bc78d759d10663da40d171dec992aa": {
        "privateKey": "e7f59a4beb997a02a13e0d5e025b39a6f0adc64d37bb1e6a849a4863b4680411",
        "amount": "1000000000000000000",
        "nonce": 0
    },
    "f6dad9e193fa2959a849b81caf9cb6ecde466771": {
        "privateKey": "589417286a3213dceb37f8f89bd164c3505a4cec9200c61f7c6db13a30a71b45",
        "amount": "1000000000000000000",
        "nonce": 0
    },
    "10200e3da08ee88729469d6eabc055cb225821e7": {
        "privateKey": "5430365143ce0154b682301d0ab731897221906a7054bbf5bd83c7663a6cbc40",
        "amount": "1000000000000000000",
        "nonce": 0
    },
    "ac941274c3b6a50203cc5e7939b7dad9f32a0c12": {
        "privateKey": "1080d2cca18ace8225354ac021f9977404cee46f1d12e9981af8c36322eac1a4",
        "amount": "1000000000000000000",
        "nonce": 0
    },
    "ec902fe17d90203d0bddd943d97b29576ece3177": {
        "privateKey": "254d9924fc1dcdca44ce92d80255c6a0bb690f867abde80e626fbfef4d357004",
        "amount": "1000000000000000000",
        "nonce": 0
    },
    "c2035715831ab100ec42e562ce341b834bed1f4c": {
        "privateKey": "b8fc4e270594d87d3f728d0873a38fb0896ea83bd6f96b4f3c9ff0a29122efe4",
        "amount": "1000000000000000000",
        "nonce": 0
    },
    "6cd3667ba79310837e33f0aecbe13688a6cbca32": {
        "privateKey": "b87f4ba7dcd6e60f2cca8352c89904e3993c5b2b0b608d255002edcda6374de4",
        "amount": "1000000000000000000",
        "nonce": 0
    }
}
```
7. Run `./build/bin/isolatedServer -f isolated-server-accounts.json &>> isolated-server.logs &`
8. You should see the following in the log `isolated-server.logs`
```
[32170][19-11-08T09:07:21.281][/AccountStore.cpp:53][AccountStore        ] Scilla IPC Server started successfully
```
9. Start development on `http://<aws ec2 url>:5555`! Use the accounts found in `./isolated-server-accounts.json`!

10. Other resources to help you get started with development!

    |          | Link(s) |
    |:---------|:-------|
    | **Official Golang SDK** | https://github.com/Zilliqa/gozilliqa-sdk |
    | **Official Javascript SDK** | https://github.com/Zilliqa/Zilliqa-JavaScript-Library |
    | **Community Ruby SDK** | https://github.com/FireStack-Lab/LaksaRuby |
    | **Community Java SDK** | https://github.com/FireStack-Lab/LaksaJ |
    | **Community Python SDK** | https://github.com/deepgully/pyzil |
    | **JSON RPC API Docs** | https://apidocs.zilliqa.com/ |
    | **Smart Contract IDE** | https://ide.zilliqa.com/ |
    | **Testnet Wallet** | https://dev-wallet.zilliqa.com/home |

### Acknowledgements

Isolated server is created by: @KaustubhShamshery
Documentation is written by: @charlieysc
