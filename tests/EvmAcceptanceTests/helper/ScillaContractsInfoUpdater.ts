import {glob} from "glob";
import fs from "fs";
import path, {dirname} from "path";
import {createHash} from "crypto";
import {ContractName, Transitions, parseScilla, Fields, ParsedContract} from "./ScillaParser";
import clc from "cli-color";

// For some reason, hardhat deletes json files in artifacts, so it couldn't be scilla.json
const CONTRACTS_INFO_CACHE_FILE = path.join(__dirname, "../artifacts/scilla.cache");

export interface ContractInfo {
  hash: string;
  path: string;
  parsedContract: ParsedContract;
}

type ContractPath = string;
export type ContractMapByName = {[key: ContractName]: ContractInfo};
type ContractMapByPath = {[key: ContractPath]: ContractInfo};

export const updateContractsInfo = () => {
  let contractsInfo: ContractMapByName = {};
  let files = glob.sync("contracts/**/*.scilla");
  if (files.length === 0) {
    console.log(clc.yellowBright("No scilla contracts were found in contracts directory."));
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
    console.log(`Parsing ${file}...`);
    if (contract) {
      somethingChanged = true;
      contractsInfo[file] = contract;
    } else {
      console.log(clc.redBright("  Failed!"));
    }
  });

  if (somethingChanged) {
    console.log("Cache updated.");
    saveContractsInfo(contractsInfo);
  } else {
    console.log("Nothing changed since last compile.");
  }
};

export const loadScillaContractsInfo = (): ContractMapByName =>  {
  const contractsInfo = loadContractsInfo();
  return convertToMapByName(contractsInfo);
}

const convertToMapByName = (contracts: ContractMapByPath): ContractMapByName => {
  let contractsByName: ContractMapByName = {};
  for (let key in contracts) {
    contractsByName[contracts[key].parsedContract.name] = contracts[key];
  }

  return contractsByName;
};

const loadContractsInfo = (): ContractMapByPath => {
  if (!fs.existsSync(CONTRACTS_INFO_CACHE_FILE)) {
    console.log("Cache file doesn't exist, creating a new one");
    return {};
  }

  let contents = fs.readFileSync(CONTRACTS_INFO_CACHE_FILE, "utf8");
  return JSON.parse(contents);
};

const saveContractsInfo = (contracts: ContractMapByPath) => {
  fs.mkdirSync(dirname(CONTRACTS_INFO_CACHE_FILE), {recursive: true});
  fs.writeFileSync(CONTRACTS_INFO_CACHE_FILE, JSON.stringify(contracts));
};

const getFileHash = (fileName: string): string => {
  let contents = fs.readFileSync(fileName, "utf8");
  const hashSum = createHash("md5");
  hashSum.update(contents);
  return hashSum.digest("hex");
};

const parseScillaFile = (fileName: string): ContractInfo | null => {
  let contents = fs.readFileSync(fileName, "utf8");
  const hashSum = createHash("md5");
  hashSum.update(contents);

  const parsedContract = parseScilla(fileName);

  return {hash: hashSum.digest("hex"), path: fileName, parsedContract};
};
