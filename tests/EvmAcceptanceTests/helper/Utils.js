var axios = require('axios'),
    _ = require('underscore')

var Utils = {
    getEthChainId: function() {
        return hre.network.config.chainId;
    },

    getZilliqaChainId: function() {
        return hre.network.config.chainId - 0x8000;
    },

    getNetworkUrl: function() {
        return hre.network.config.url
    },

    getPrivateAddress: function(index) {
        return hre.network.config.accounts[index]
    },

    callEthMethod: async function(method, id, params, callback) {
        const data = {
            id: id,
            jsonrpc: "2.0",
            method: method,
            params: params
        }
    
        const host = this.getNetworkUrl()

        // ASYNC
        if(typeof callback === 'function') {
            await axios.post(host, data).then(response => {
                console.log(response)
                if(response.status === 200) {
                    callback(response.data, response.status);
                } else {
                    throw new Error('Can\'t connect to '+ host + "\n Send: "+ JSON.stringify(data, null, 2));
                }
            })
        // SYNC
        } else {
            const response = await axios.post(host, data)

            if(response.status !== 200) {
                throw new Error('Can\'t connect to '+ host + "\n Send: "+ JSON.stringify(data, null, 2));
            }

            return response.data
        }
    },

    isAddress: function(address) {
        if (!_.isString(address)) {
            return false;
        }

        return (/^[x0-9a-f]{0,42}$/i.test(address) && (address.length === 42|| address.length === 40));
    },
}

module.exports = Utils