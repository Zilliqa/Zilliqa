var Node = require('./node')
var util = require('./util')
var config = require('./config.json')
var ajax = util.ajax
var validateArgs = util.validateArgs


function zLib (args) {
  validateArgs(args, {}, {
    nodeUrl: [util.isUrl]
  })

  this.version = config.version
  this.node = new Node({url: (args.nodeUrl || config.defaultNodeUrl)})
	this.data = {}
}


// library methods
zLib.prototype.getLibraryVersion = function () {
  return this.version
}

zLib.prototype.isConnected = function () {
  return (this.node && this.node.isConnected())
}

zLib.prototype.getNode = function () {
  return this.node
}

zLib.prototype.setNode = function (args) {
  validateArgs(args, {
    nodeUrl: [util.isUrl]
  })

  this.node = new Node(args.nodeUrl)
  return null
}


module.exports = zLib
