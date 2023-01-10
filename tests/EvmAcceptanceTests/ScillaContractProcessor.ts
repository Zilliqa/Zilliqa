import {glob} from "glob";
import fs from "fs";
import path from "path";
import {createHash} from "crypto";
import {ContractName, Transitions, parseScilla, Fields} from "./helper/ScillaParser";

// For some reason, hardhat deletes json files in artifacts, so it couldn't be scilla.json
const CONTRACTS_INFO_CACHE_FILE = "./artifacts/scilla.cache";

export interface ContractInfo {
  hash: string;
  name: ContractName;
  path: string;
  transitions: Transitions;
  fields: Fields;
}

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

  let somethingChanged = false;
  files.forEach((file) => {
    if (file in contractsInfo && contractsInfo[file].hash === getFileHash(file)) {
      return; // Nothing to do
    }

    // Either the file is new or has been changed
    const contract = parseScillaFile(file);
    if (contract) {
      somethingChanged = true;
      contractsInfo[file] = contract;
    }
  });

  if (somethingChanged) {
    saveContractsInfo(contractsInfo);
  }

  scillaContracts = convertToMapByName(contractsInfo);
};

const convertToMapByName = (contracts: ContractMapByPath): ContractMapByName => {
  let contractsByName: ContractMapByName = {};
  for (let key in contracts) {
    contractsByName[contracts[key].name] = contracts[key];
  }

  return contractsByName;
};

const loadContractsInfo = (): ContractMapByPath => {
  if (!fs.existsSync(CONTRACTS_INFO_CACHE_FILE)) {
    return {};
  }

  let contents = fs.readFileSync(CONTRACTS_INFO_CACHE_FILE, "utf8");
  return JSON.parse(contents);
};

const saveContractsInfo = (contracts: ContractMapByPath) => {
  fs.writeFileSync(path.join(__dirname, CONTRACTS_INFO_CACHE_FILE), JSON.stringify(contracts));
};

const getFileHash = (fileName: string): string => {
  let contents = fs.readFileSync(fileName, "utf8");
  const hashSum = createHash("md5");
  hashSum.update(contents);
  return hashSum.digest("hex");
};

const parseScillaFile = (fileName: string): ContractInfo | null => {
  console.log("parse " + fileName);
  let contents = fs.readFileSync(fileName, "utf8");
  const hashSum = createHash("md5");
  hashSum.update(contents);

  const [contractName, transitions, fields] = parseScilla(fileName);

  return {name: contractName, hash: hashSum.digest("hex"), path: fileName, transitions, fields};
};

updateContractsInfo();
