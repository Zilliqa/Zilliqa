var assert = require('assert')
var zWallet = require('../index')

var zlib = new zLib({nodeUrl: 'http://localhost:8123'})

describe('z-lib', function() {
  describe('#getClientVersion()', function() {
    it('should return the current client version', function() {
    })
  })

  describe('#getNetwordId()', function() {
    it('should return network id client is connected to', function() {
    })
  })

  describe('#createTransaction()', function() {
    it('should create a transaction and return the transaction hash', function() {
    })
  })

  describe('#getTransaction()', function() {
    it('should return transaction when passed transaction hash', function() {
    })
    it('should return transaction when passed block hash and transaction index', function() {
    })
    it('should return transaction when passed block number and transaction index', function() {
    })
  })

  describe('#getDsBlock()', function() {
    it('should return DS block when passed block hash', function() {
    })
    it('should return DS block when passed block number', function() {
    })
  })

  describe('#getTxBlock()', function() {
    it('should return TX block when passed block hash', function() {
    })
    it('should return TX block when passed block number', function() {
    })
  })

  describe('#getLatestDsBlock()', function() {
    it('should return the latest DS block', function() {
    })
  })

  describe('#getLatestTxBlock()', function() {
    it('should return the latest TX block', function() {
    })
  })

  describe('#getBalance()', function() {
    it('should return the balance of the account of the passed address', function() {
    })
  })

  describe('#getGasPrice()', function() {
    it('should return the balance of the account of the passed address', function() {
    })
  })

  describe('#getStorageAt()', function() {
    it('should return the balance of the account of the passed address', function() {
    })
  })

  describe('#getTransactionHistory()', function() {
    it('should return the balance of the account of the passed address', function() {
    })
  })

  describe('#getBlockTransactionCount()', function() {
    it('should return the balance of the account of the passed address', function() {
    })
  })

  describe('#getCode()', function() {
    it('should return the balance of the account of the passed address', function() {
    })
  })

  describe('#createMessage()', function() {
    it('should return the balance of the account of the passed address', function() {
    })
  })

  describe('#getGasEstimate()', function() {
    it('should return the estimated gas required for a transaction', function() {
    })
  })

  describe('#getTransactionReceipt()', function() {
    it('should return the receipt of a transaction', function() {
    })
  })

  describe('#getHashrate()', function() {
    it('should return the number of hashes per second', function() {
    })
  })

  describe('#isNodeMining()', function() {
    it('should return true if client is actively mining new blocks', function() {
    })
  })

  describe('#compileCode()', function() {
    it('should return compiled smart contract code', function() {
    })
  })
})
