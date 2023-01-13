const parse: any = require("s-expression");
import fs from "fs";
import {execSync} from "child_process";

type ScillaDataType = string;

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

export interface ParsedContract {
  name: ContractName;
  constructorParams: Fields | null;
  transitions: Transitions;
  fields: Fields;
}

export const parseScilla = (filename: string): ParsedContract => {
  if (!fs.existsSync(filename)) {
    throw new Error(`${filename} doesn't exist.`);
  }

  const sexp = execSync(`scilla-fmt --sexp --human-readable ${filename}`);

  const result: any[] = parse(sexp.toString());
  const contr = result.filter((row: string[]) => row[0] === "contr")[0][1];

  const contractName = extractContractName(contr);
  const contractParams = extractContractParams(contr);

  const cfields = contr.filter((row: string[]) => row[0] === "cfields")[0][1];
  const fields = extractContractFields(cfields);

  const ccomps = contr.filter((row: string[]) => row[0] === "ccomps")[0][1];
  const transitions = extractTransitions(ccomps);

  return {name: contractName, transitions, fields, constructorParams: contractParams};
};

const extractContractName = (contrElem: any[]): ContractName => {
  return contrElem
    .filter((row: string[]) => row[0] === "cname")[0][1]
    .filter((row: string[]) => row[0] === "SimpleLocal")[0][1];
};

const extractContractParams = (contrElem: any[]): Fields | null => {
  if (contrElem[1][0] !== "cparams") {
    throw new Error(`Index 0 is not cparams: ${contrElem}`);
  }

  if (contrElem[1][1].length === 0) {
    return null;
  }

  return extractContractFields(contrElem[1][1]);
};

const extractContractFields = (cfieldsElem: any[]): Fields => {
  return cfieldsElem.map((row: any[]): Field => {
    const identData = row[0];
    if (identData[0] !== "Ident") {
      throw new Error(`Index 0 is not Ident: ${identData}`);
    }

    const fieldNameData = identData[1];
    if (fieldNameData[0] !== "SimpleLocal") {
      throw new Error(`Index 0 is not SimpleLocal: ${fieldNameData}`);
    }

    const fieldTypeData = row[1];
    // Currently we just parse PrimType, for the rest we don't parse it completely.
    if (fieldTypeData[0] === "PrimType") {
      return {
        type: fieldTypeData[1],
        name: fieldNameData[1]
      };
    } else if (fieldTypeData[0] === "ADT") {
      return {
        type: "ADT",
        name: fieldNameData[1]
      };
    } else if (fieldTypeData[0] === "MapType") {
      return {
        type: "Map",
        name: fieldNameData[1]
      };
    } else if (fieldTypeData[0] === "Address") {
      return {
        type: "Address",
        name: fieldNameData[1]
      };
    } else {
      throw new Error(`Data type is unknown: ${fieldTypeData}`);
    }
  });
};

const extractTransitions = (ccompsElem: any[]): Transitions => {
  return ccompsElem.map((row: any[]) => {
    const compTypeData = row[0];
    if (compTypeData[0] !== "comp_type") {
      throw new Error(`Index 0 is not comp_type ${compTypeData}`);
    }
    const compType = compTypeData[1];

    const compNameData = row[1];
    if (compNameData[0] !== "comp_name") {
      throw new Error(`Index 0 is not comp_name ${compNameData}`);
    }

    const compName = compNameData[1][1];
    if (compName[0] !== "SimpleLocal") {
      throw new Error(`Index 0 is not SimpleLocal: ${compName}`);
    }

    const compParamsData = row[2];

    if (compParamsData[0] !== "comp_params") {
      throw new Error(`Index 0 is not comp_params: ${compParamsData}`);
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
};
