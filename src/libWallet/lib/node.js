var util = require('./util')
var validateArgs = util.validateArgs
var $ = require('jquery')


function Node (args) {
  validateArgs(args, {
    url: [util.isUrl]
  })

  this.url = args.url
}

function rpcAjax(url, method, params, cb) {
  return $.ajax({
    url: url,
    type: 'POST',
    dataType: 'json',
    data: JSON.stringify({
      jsonrpc: '2.0',
      method: method,
      params: [params],
      id: 1
    })
  }).done(function(data) {
    cb(null, data);
  }).fail(function(err) {
    cb(err);
  })
}

// helper methods
Node.prototype.isConnected = function (cb) {
  rpcAjax(this.url, 'isConnected', [], cb)
}


// API methods
Node.prototype.getClientVersion = function (cb) {
  rpcAjax(this.url, 'getClientVersion', [], cb)
}

Node.prototype.getNetworkId = function (cb) {
  rpcAjax(this.url, 'getNetworkId', [], cb)
}

Node.prototype.getProtocolVersion = function (cb) {
  rpcAjax(this.url, 'getProtocolVersion', [], cb)
}

Node.prototype.createTransaction = function (args, cb) {
  validateArgs(args, {
    nonce: [util.isNumber],
    to: [util.isAddress],
    pubKey: [util.isECsig],
    amount: [util.isNumber],
    gasPrice: [util.isNumber],
    gasLimit: [util.isNumber]
  })

  rpcAjax(this.url, 'createTransaction', args, cb)
  // args is the transaction object
  // return '0x7f9fade1c0d57a7af66ab4ead7c2eb7b11a91385'
}

Node.prototype.getTransaction = function (args, cb) {
  // args can be transaction hash/block hash+index/block number+index
  validateArgs(args, {}, {
    txHash: [util.isTxHash],
    blockHash: [util.isBlockHash],
    blockNumber: [util.isBlockNumber],
    index: [util.isIndex]
  })

  txData = {}
  if (args.txHash)
    txData.txHash = args.txHash
  if (args.blockHash && args.index)
    txData.blockHash = args.blockHash
  if (args.blockNumber && args.index)
    txData.blockNumber = args.blockNumber

  if ($.isEmptyObject(txData))
    throw new Error('Incomplete args passed')

  rpcAjax(this.url, 'getTransaction', txData, cb)
  // return {
  //   "version": '0.0.1',
  //   "nonce": "0xfb6e1a62d119228b",
  //   "to": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b",
  //   "from": "0xa94f5374fce5edbc8e2a8697c15331677e6ebf0b",
  //   "amount": 43464,
  //   "pubKey": "23r45325345",
  //   "signature": "W29iamVjdCBPYmplY3Rd"
  // }
}

Node.prototype.getDsBlock = function (args, cb) {
  // can pass in  block hash or block number
  validateArgs(args, {}, {
    blockHash: [util.isBlockHash],
    blockNumber: [util.isBlockNumber]
  })
  var params = args.blockHash || args.blockNumber
  
  rpcAjax(this.url, 'getDsBlock', params, cb)

  // return {
  //   "header": {
  //     "version": '0.0.1',
  //     "previousHash": "0xef95f2f1ed3ca60b048b4bf67cde2195961e0bba6f70bcbea9a2c4e133e34b46",
  //     "pubKey": "W29iamVjdCBPYmplY3Rd",
  //     "difficulty": 0x001340224,
  //     "number": 3462,
  //     "timestamp": 1429287689,
  //     "mixHash": "0xef95f2f1ed3ca60b048b4bf67cde2195961e0bba6f70bcbea9a2c4e133e34b46",
  //     "nonce": "0xfb6e1a62d119228b"
  //   },
  //   "signature": {
  //     "signature": "0xfb6e1a62d119228b",
  //     "bitmap": [0,1,0,1]
  //   }
  // }
}

Node.prototype.getTxBlock = function (args, cb) {
  // can pass in  block hash or block number
  validateArgs(args, {}, {
    blockHash: [util.isBlockHash],
    blockNumber: [util.isBlockNumber]
  })
  var params = args.blockHash || args.blockNumber
  
  rpcAjax(this.url, 'getTxBlock', params, cb)
  // return {
  //   "header": {
  //     "type": '1',
  //     "version": '0.0.1',
  //     "previousHash": "0xef95f2f1ed3ca60b048b4bf67cde2195961e0bba6f70bcbea9a2c4e133e34b46",
  //     "gasLimit": 21662,
  //     "gasUsed": 2154,
  //     "stateRoot": "0x3a1b03875115b79539e5bd33fb00d8f7b7cd61929d5a3c574f507b8acf415bee",
  //     "transactionRoot": "0x3a1b03875115b79539e5bd33fb00d8f7b7cd61929d5a3c574f507b8acf415bee",
  //     "txHashes": [],
  //     "pubKey": "123456789",
  //     "pubKeyMicroBlocks": [],
  //     "parentBlockHash": "0xef95f2f1ed3ca60b048b4bf67cde2195961e0bba6f70bcbea9a2c4e133e34b46",
  //     "parentDsHash": "0xef95f2f1ed3ca60b048b4bf67cde2195961e0bba6f70bcbea9a2c4e133e34b46",
  //     "parentDsBlockNumber": 101,
  //     "number": 102,
  //     "timestamp": 1429287689,
  //     "nonce": "0xfb6e1a62d119228b"
  //   },
  //   "data": {
  //     "txCount": 3,
  //     "txList": ['0x9fc76417374aa880d4449a1f7f31ec597f00b1f6f3dd2d66f4c9c6c445836d8b']
  //   },
  //   "signature": {
  //     "signature": "0xfb6e1a62d119228b",
  //     "bitmap": [1,0,0,1]
  //   }
  // }
}

Node.prototype.getLatestDsBlock = function (cb) {
  rpcAjax(this.url, 'getLatestDsBlock', "", cb)
  // return {
  //   "header": {
  //     "version": '0.0.1',
  //     "previousHash": "0xef95f2f1ed3ca60b048b4bf67cde2195961e0bba6f70bcbea9a2c4e133e34b46",
  //     "pubKey": "W29iamVjdCBPYmplY3Rd",
  //     "difficulty": 0x001340224,
  //     "number": 3462,
  //     "timestamp": 1429287689,
  //     "mixHash": "0xef95f2f1ed3ca60b048b4bf67cde2195961e0bba6f70bcbea9a2c4e133e34b46",
  //     "nonce": "0xfb6e1a62d119228b"
  //   },
  //   "signature": {
  //     "signature": "0xfb6e1a62d119228b",
  //     "bitmap": [0,1,0,1]
  //   }
  // }
}

Node.prototype.getLatestTxBlock = function (cb) {
  rpcAjax(this.url, 'getLatestTxBlock', "", cb)
  // return {
  //   "header": {
  //     "type": '1',
  //     "version": '0.0.1',
  //     "previousHash": "0xef95f2f1ed3ca60b048b4bf67cde2195961e0bba6f70bcbea9a2c4e133e34b46",
  //     "gasLimit": 21662,
  //     "gasUsed": 2154,
  //     "stateRoot": "0x3a1b03875115b79539e5bd33fb00d8f7b7cd61929d5a3c574f507b8acf415bee",
  //     "transactionRoot": "0x3a1b03875115b79539e5bd33fb00d8f7b7cd61929d5a3c574f507b8acf415bee",
  //     "txHashes": [],
  //     "pubKey": "123456789",
  //     "pubKeyMicroBlocks": [],
  //     "parentBlockHash": "0xef95f2f1ed3ca60b048b4bf67cde2195961e0bba6f70bcbea9a2c4e133e34b46",
  //     "parentDsHash": "0xef95f2f1ed3ca60b048b4bf67cde2195961e0bba6f70bcbea9a2c4e133e34b46",
  //     "parentDsBlockNumber": 101,
  //     "number": 102,
  //     "timestamp": 1429287689,
  //     "nonce": "0xfb6e1a62d119228b"
  //   },
  //   "data": {
  //     "txCount": 3,
  //     "txList": ['0x9fc76417374aa880d4449a1f7f31ec597f00b1f6f3dd2d66f4c9c6c445836d8b']
  //   },
  //   "signature": {
  //     "signature": "0xfb6e1a62d119228b",
  //     "bitmap": [1,0,0,1]
  //   }
  // }
}

Node.prototype.getBalance = function (args, cb) {
  validateArgs(args, {
    address: [util.isAddress]
  })

  rpcAjax(this.url, 'getBalance', args.address, cb)
}

Node.prototype.getGasPrice = function (cb) {
  rpcAjax(this.url, 'getGasPrice', "", cb)
}

Node.prototype.getStorageAt = function (args, cb) {
  validateArgs({
    address: [util.isAddress],
    index: [util.isNumber]
  })

  rpcAjax(this.url, 'getStorageAt', args, cb)
}

Node.prototype.getTransactionHistory = function (args, cb) {
  validateArgs(args, {
    address: [util.isAddress]
  })
  
  rpcAjax(this.url, 'getTransactionHistory', args.address, cb)
}

Node.prototype.getBlockTransactionCount = function (args, cb) {
  // can pass in  block hash or block number
  validateArgs(args, {}, {
    blockHash: [util.isBlockHash],
    blockNumber: [util.isBlockNumber]
  })
  var params = args.blockHash || args.blockNumber
  
  rpcAjax(this.url, 'getBlockTransactionCount', params, cb)
}

Node.prototype.getCode = function (args, cb) {
  validateArgs(args, {
    address: [util.isAddress]
  })

  rpcAjax(this.url, 'getCode', args.address, cb)
  // return '0x605880600c6000396000f3006000357c010000000000000000000000000000000000000000000000000000000090048063c6888fa114602e57005b603d6004803590602001506047565b8060005260206000f35b60006007820290506053565b91905056'
}

Node.prototype.createMessage = function (args, cb) {
  validateArgs({
    to: [util.isAddress]
  }, {
    from: [util.isAddress],
    gas: [util.isNumber],
    gasPrice: [util.isNumber]
  })

  rpcAjax(this.url, 'createMessage', args, cb)
}

Node.prototype.getGasEstimate = function (args, cb) {
  validateArgs({}, {
    to: [util.isAddress],
    from: [util.isAddress],
    gas: [util.isNumber],
    gasPrice: [util.isNumber],
    gasLimit: [util.isNumber]
  })

  rpcAjax(this.url, 'getGasEstimate', args, cb)
}

Node.prototype.getTransactionReceipt = function (args, cb) {
  validateArgs(args, {
    txHash: [util.isTxHash]
  })

  rpcAjax(this.url, 'getTransactionReceipt', args.txHash, cb)

  // return {
    // 'transactionHash': '0x9fc76417374aa880d4449a1f7f31ec597f00b1f6f3dd2d66f4c9c6c445836d8b',
    // 'transactionIndex': 3,
    // 'blockHash': '0xef95f2f1ed3ca60b048b4bf67cde2195961e0bba6f70bcbea9a2c4e133e34b46',
    // 'blockNumber': 33,
    // 'cumulativeGasUsed': '600',
    // 'gasUsed': '50',
    // 'contractAddress': null
  // }
}

Node.prototype.getHashrate = function (cb) {
  rpcAjax(this.url, 'getHashrate', "", cb)
}

Node.prototype.isNodeMining = function (args, cb) {
  rpcAjax(this.url, 'isNodeMining', "", cb)
}

Node.prototype.compileCode = function (args, cb) {
  validateArgs({
    code: [util.isString]
  })

  rpcAjax(this.url, 'compileCode', args, cb)
  // return {
  //   "code": "0x605880600c6000396000f3006000357c010000000000000000000000000000000000000000000000000000000090048063c6888fa114602e57005b603d6004803590602001506047565b8060005260206000f35b60006007820290506053565b91905056",
  //   "metadata": {
  //     "source": "print sum(5, 6)"
  //   }
  // }
}


module.exports = Node
