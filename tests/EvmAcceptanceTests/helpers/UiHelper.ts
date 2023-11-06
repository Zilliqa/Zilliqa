import input from "@inquirer/input";
import select from "@inquirer/select";
import {Account, AccountType} from "./SignersHelper";

export const askAmount = async (message?: string): Promise<number> => {
  return Number(await input({message: message || "How much for each signer?"}));
};

export const askForAccount = async (typeMessage?: string, privateKeyMessage?: string): Promise<Account> => {
  const type = await askForAccountType(typeMessage);
  const private_key = await input({message: privateKeyMessage || "Enter your private key: "});
  return {
    type,
    private_key
  };
};

export const askForAccountType = async (
  message?: string,
  ethMessage?: string,
  zilMessage?: string
): Promise<AccountType> => {
  return await select({
    message: message || "What type of funded source account do you have?",
    choices: [
      {
        name: ethMessage || "An eth-based account",
        value: AccountType.EthBased
      },
      {
        name: zilMessage || "A zil-based account",
        value: AccountType.ZilBased
      }
    ]
  });
};

export const askForAddress = async (message?: string): Promise<string> => {
  return await input({message: message || "Enter the destination address: "});
};
