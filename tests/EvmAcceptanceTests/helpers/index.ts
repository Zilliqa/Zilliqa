import {HardhatNetworkAccountConfig, HardhatNetworkAccountsConfig, HttpNetworkAccountsConfig} from "hardhat/types";

export * from "./DebugHelper";
export * from "./JsonRpcHelper";
export * from "./Parallelizer";
export * from "./SignerPool";
export * from "./parallel-tests/Scenario";
export * from "./parallel-tests/Worker";
export * from "./parallel-tests/Display";
export * from "./parallel-tests/Stage";

export function isHardhatNetworkAccountConfig(
  config: HardhatNetworkAccountsConfig | HttpNetworkAccountsConfig
): config is HardhatNetworkAccountConfig[] {
  return (<HardhatNetworkAccountConfig[]>config)[0].privateKey !== undefined;
}
