const parse: any = require("s-expression");
import fs from "fs";
import {execSync} from "child_process";

export type ScillaDataType =
  | "Int32"
  | "Int64"
  | "Int128"
  | "Int256"
  | "Uint32"
  | "Uint64"
  | "Uint128"
  | "Uint256"
  | "String"
  | "BNum"
  | "Message"
  | "Event"
  | "Exception"
  | "ReplicateContr"
  | "ByStr";

export const isNumeric = (type: ScillaDataType) => {
  switch (type) {
    case "Int64":
    case "Int128":
    case "Int256":
    case "Uint32":
    case "Uint64":
    case "Uint128":
    case "Uint256":
      return true;

    default:
      return false;
  }
};

export interface TransitionParam {
  type: ScillaDataType;
  name: string;
}

export interface Transition {
  type: string;
  name: string;
  params: TransitionParam[];
}

export interface Field {
  type: ScillaDataType;
  name: string;
}

export type Transitions = Transition[];
export type ContractName = string;
export type Fields = Field[];

export const parseScilla = (filename: string): [ContractName, Transitions, Fields] => {
  if (!fs.existsSync(filename)) {
    throw new Error(`${filename} doesn't exist.`);
  }

  const sexp = execSync(`scilla-fmt --sexp --human-readable ${filename}`);

  const result: any[] = parse(sexp.toString());
  const contr = result.filter((row: string[]) => row[0] === "contr")[0][1];
  const contractName = contr
    .filter((row: string[]) => row[0] === "cname")[0][1]
    .filter((row: string[]) => row[0] === "SimpleLocal")[0][1];

  const cfields = contr.filter((row: string[]) => row[0] === "cfields")[0][1];
  const fields = cfields.map((row: any[]): Field => {
    const identData = row[0];
    if (identData[0] !== "Ident") {
      throw new Error("0 is not Indent");
    }

    const fieldNameData = identData[1];
    if (fieldNameData[0] !== "SimpleLocal") {
      throw new Error("0 is not SimpleLocal");
    }

    const fieldTypeData = row[1];
    if (fieldTypeData[0] !== "PrimType") {
      throw new Error("0 is not PrimType");
    }

    return {
      type: fieldTypeData[1],
      name: fieldNameData[1]
    };
  });

  const ccomps = contr.filter((row: string[]) => row[0] === "ccomps")[0][1];
  const functions = ccomps.map((row: any[]) => {
    const comp_type_data = row[0];
    if (comp_type_data[0] !== "comp_type") {
      throw new Error("0 is not comp_type");
    }
    const compType = row[0][1];

    const comp_name_data = row[1];
    if (comp_name_data[0] !== "comp_name") {
      throw new Error("0 is not comp_name");
    }

    const compName = row[1][1][1];
    if (compName[0] !== "SimpleLocal") {
      throw new Error("0 is not SimpleLocal");
    }

    const compParamsData = row[2];

    if (compParamsData[0] !== "comp_params") {
      throw new Error("0 is not comp_params");
    }

    const compParams = compParamsData[1].map((row: any[][][]) => {
      const primType = row[1][1];
      const primName = row[0][1][1];
      return {
        type: primType,
        name: primName
      };
    });
    return {
      type: compType,
      name: compName[1],
      params: compParams
    };
  });

  return [contractName, functions, fields];
};

console.log(parseScilla("./contracts/scilla/SetGet.scilla"));
