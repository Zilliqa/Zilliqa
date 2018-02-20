module.exports = {

	// make sure each of the keys in requiredArgs is present in args
	// and each of it's validator functions return true
	validateArgs: function (args, requiredArgs, optionalArgs) {
		for(var key in requiredArgs) {
			if (args[key] !== undefined)
				throw new Error('Key not found: ' + key)

			for(var i = 0 ; i < requiredArgs[key].length ; i++) {
				if (typeof(requiredArgs[key][i]) != 'function')
					throw new Error('Validator is not a function')

				if (!requiredArgs[key][i](args[key]))
					throw new Error('Validation failed for ' + key)
			}
		}

		for(var key in optionalArgs) {
			if (args[key]) {
				for(var i = 0 ; i < optionalArgs[key].length ; i++) {
					if (typeof(optionalArgs[key][i]) != 'function')
						throw new Error('Validator is not a function')

					if (!optionalArgs[key][i](args[key]))
						throw new Error('Validation failed for ' + key)
				}
			}
		}
		return true
	},

	isAddress: function(address) {
		return (typeof(address) == 'string' && address.substr(0, 2) == '0x')
	},

	isUrl: function(url) {
		return (typeof(url) == 'string')
	},

	isTxHash: function(blockHash) {
		return (typeof(blockHash) == 'string')
	},

	isBlockHash: function(blockHash) {
		return (typeof(blockHash) == 'string')
	},

	isBlockNumber: function(blockNumber) {
		return (typeof(blockNumber) == 'string')
	},

	isECsig: function(signature) {
		return (typeof(signature) == 'string')
	},

	isNumber: function(number) {
		return (typeof(number) == 'number')
	},

	isString: function(string) {
		return (typeof(string) == 'string')
	}
}