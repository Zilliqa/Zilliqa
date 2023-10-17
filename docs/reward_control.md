# Reward control

The reward control contract manages rewards on chain - see ZIP-24 for details.

## Deploying in production

In production:

 * Clone `zilliqa-developer`.
 * Deploy the contracts in `zilliqa-developer/contracts/reward_control`
 * Use the IDE (or other tool) to change ownership of the multisig to the custodian addresses (available on the wiki if you're in Zilliqa)
 * List the address of the reward contract (*NOT* the multisig) in `constants.xml`
 * You're good to go.

## Testing reward control

Zilliqa 1 now comes with built-in reward control. It works like this:

 * Deploy the contract in
   `tests/EvmAcceptanceTests/contracts/scilla/RewardsParams.scilla` to
   a known address (you can do this with a new account, since contract
   addresses only depend on the account address and the nonce).

 * List that address as `REWARD_CONTROL_CONTRACT_ADDRESS` in `constants.xml`

 * The chain will then pick up the relevant parameters (by name!) from
   the reward control contract.

Testing this is, unfortunately, not straightforward. However, there is a hardhat task to help you in `test/EvmAcceptanceTests/tasks/SetRewards.ts`.

Before you start, you may want to set `NUM_FINAL_BLOCK_PER_POW` to 5 or 10 (to make reward cycles more frequent).

You should:

  * Run a network with `REWARD_CONTROL_CONTRACT_ADDRESS` not set, to check that the reward behaviour remains unchanged.
  * Test that if there is no entry in config.xml for `REWARD_EACH_MUL_IN_MILLIS` and `BASE_REWARD_MUL_IN_MILLIS`, the correct defaults (1668 and 4726 respectively) are substituted.
  * Check that `ENABLE_REAWRD_DEBUG_FILE` operates correctly (the `rewards.txt` file should be written when `true` and not written otherwise).
  * Now, set `REWARD_CONTROL_CONTRACT_ADDRESS` to the magic string `0xb73da094d60aa93ac4fa8ae41df5d8c13925b0bd` (you don't need the contract deployed at this point)
  * Set `PRIMARY_ACCOUNT` to an account with a suitable number of ZIL (one of the isolated server accounts will do).
  * Run `npx hardhat set-rewards` with
     * `--coinbaserewardperds <NNN>`
     * `--baserewardscaledpercent <NNN>`
     * `--lookuprewardsscaledpercent <NNN>`
     * `--rewardeachmillis <NNN>`
     * `--baserewardmillis <NNN>`
  * Verify that the changed rewards are correctly implemented.

`npx hardhat set-rewards` will auto-deploy the contract (with the
well-known `0xb73..` address) and then call any transitions you've
asked for - in series, since with low values of
`NUM_FINAL_BLOCK_FOR_ROW`, issuing transactions ahead doesn't seem to
work.

## Testing diagnostic file output

Turn `ENABLE_REWARD_DEBUG_FILE` on; observe that output is generated.

Turn `ENABLE_REWARD_DEBUG_FILE` off; observe that it isn't any longer.


## Upgrades

To upgrade the contract, you:

 * Deploy a new contract
 * Write code in `Coinbase.cpp` which deals (in a backward compatible way) with the new contract.
 * Change `config.xml` in a hard-fork upgrade.
 
