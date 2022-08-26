const Web3 = require('web3');
const web3 = new Web3();

const calldata = web3.eth.abi.encodeFunctionCall({name:'store', type:'function', inputs:[{name:'num1', type:'uint256'}]}, [12345]);


console.log(calldata);

