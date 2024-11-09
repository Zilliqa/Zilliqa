const validEip1898Complete = {
  blockHash: "0xb3b20624d03e6a530dd9e5f234233dd6249c9f8a248c7f4acb75cfc2b4209d63",
  blockNumber: "0x64"
};

const validEip1898WithHash = {
  blockHash: "0xb3b20624d03e6a530dd9e5f234233dd6249c9f8a248c7f4acb75cfc2b4209d63"
};

const validEip1898WithNumber = {
  blockNumber: "0x64"
};

const invalidEip1898Empty = {};

const invalidEip1898IncorrectKeys = {
  blockkkkHash: "0xb3b20624d03e6a530dd9e5f234233dd6249c9f8a248c7f4acb75cfc2b4209d63",
  blockkkkNumber: "0x64"
};

module.exports = {
  validEip1898Complete,
  validEip1898WithHash,
  validEip1898WithNumber,
  invalidEip1898Empty,
  invalidEip1898IncorrectKeys
};
