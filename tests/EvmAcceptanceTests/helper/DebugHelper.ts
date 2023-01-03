import hre from "hardhat";

const logDebug = hre.debug ? console.log.bind(console) : function () {};
export default logDebug;
