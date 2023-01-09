import {glob} from "glob";
import fs from "fs";
import {createHash} from "crypto";

const CONTRACTS_INFO_CACHE_FILE = "./artifacts/scilla.json"

export interface ContractInfo {
  hash: string;
  name: string;
  path: string;
}

type ContractName = string;
type ContractPath = string;
type ContractMapByName = {[key: ContractName]: ContractInfo};
type ContractMapByPath = {[key: ContractPath]: ContractInfo};

export let scillaContracts: ContractMapByName = {};

export const updateContractsInfo = () => {
  let contractsInfo: ContractMapByName = {};
  let files = glob.sync("contracts/**/*.scilla");
  if (files.length === 0) {
    return;
  }

  contractsInfo = loadContractsInfo();
  
  files.forEach((file)=>{
    if (file in contractsInfo && contractsInfo[file].hash === getFileHash(file)) {
      return;   // Nothing to do
    }
    
    // Either the file is new or has been changed
    const contract = parseScillaFile(file);
    if (contract) {
      contractsInfo[file] = contract;
    }
  });

  saveContractsInfo(contractsInfo);

  scillaContracts = convertToMapByName(contractsInfo);
}

const convertToMapByName = (contracts: ContractMapByPath): ContractMapByName => {
  let contractsByName: ContractMapByName = {};
  for (let key in contracts) {
    contractsByName[contracts[key].name] = contracts[key];
  }

  return contractsByName;
}

const loadContractsInfo = (): ContractMapByPath => {
  if (!fs.existsSync(CONTRACTS_INFO_CACHE_FILE)) {
    return {};
  }

  let contents = fs.readFileSync(CONTRACTS_INFO_CACHE_FILE, "utf8");
  return JSON.parse(contents);
}

const saveContractsInfo = (contracts: ContractMapByPath) => {
  fs.writeFileSync(CONTRACTS_INFO_CACHE_FILE, JSON.stringify(contracts));
}

const getFileHash = (fileName: string): string => {
  let contents = fs.readFileSync(fileName, "utf8");
  const hashSum = createHash('md5');
  hashSum.update(contents);
  return hashSum.digest("hex");
}

const parseScillaFile = (fileName: string): ContractInfo | null => {
  console.log("parse " + fileName);
  let contents = fs.readFileSync(fileName, "utf8");
  const hashSum = createHash('md5');
  hashSum.update(contents);

  const contractNameRegex = /^contract (?<contractName>.+)$/gm;
  const contractNames: string[] = []
  for ( const m of contents.matchAll(contractNameRegex)) {
    if (m.groups) {
      contractNames.push(m.groups.contractName);
    }
  }

  if (contractNames.length === 0) {
    return null;   // No contract in the file
  }

  if (contractNames.length > 1) {
    throw new Error(`More than one contract is found in ${fileName}`);
  }

  return {name: contractNames[0], hash: hashSum.digest("hex"), path: fileName};
}
