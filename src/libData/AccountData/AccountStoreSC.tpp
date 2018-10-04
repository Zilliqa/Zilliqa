/**
* Copyright (c) 2018 Zilliqa
* This source code is being disclosed to you solely for the purpose of your participation in
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to
* the protocols and algorithms that are programmed into, and intended by, the code. You may
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd.,
* including modifying or publishing the code (or any part of it), and developing or forming
* another public or private blockchain network. This source code is provided ‘as is’ and no
* warranties are given as to title or non-infringement, merchantability or fitness for purpose
* and, to the extent permitted by law, all liability for your use of the code is disclaimed.
* Some programs in this code are governed by the GNU General Public License v3.0 (available at
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends
* and which include a reference to GPLv3 in their program files.
**/

#include <boost/filesystem.hpp>

#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/SafeMath.h"
#include "libUtils/SysCommand.h"

template<class MAP> AccountStoreSC<MAP>::AccountStoreSC()
{
    m_accountStoreAtomic = std::make_unique<AccountStoreAtomic<MAP>>(*this);
}

template<class MAP> void AccountStoreSC<MAP>::Init()
{
    std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);
    AccountStoreBase<MAP>::Init();
    m_curContractAddr.clear();
    m_curSenderAddr.clear();
    m_curAmount = 0;
    m_curGasLimit = 0;
    m_curGasPrice = 0;
}

template<class MAP>
bool AccountStoreSC<MAP>::UpdateAccounts(const uint64_t& blockNum,
                                         const unsigned int& numShards,
                                         const bool& isDS,
                                         const Transaction& transaction,
                                         TransactionReceipt& receipt)
{
    // LOG_MARKER();
    m_curIsDS = isDS;

    std::lock_guard<std::mutex> g(m_mutexUpdateAccounts);

    const PubKey& senderPubKey = transaction.GetSenderPubKey();
    const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
    Address toAddr = transaction.GetToAddr();

    const boost::multiprecision::uint256_t& amount = transaction.GetAmount();

    uint256_t gasRemained = transaction.GetGasLimit();

    uint256_t gasDeposit;
    if (!SafeMath<uint256_t>::mul(gasRemained, transaction.GetGasPrice(),
                                  gasDeposit))
    {
        return false;
    }

    if (transaction.GetData().empty() && transaction.GetCode().empty())
    {
        // LOG_GENERAL(INFO, "Normal transaction");

        // Disallow normal transaction to contract account
        Account* toAccount = this->GetAccount(toAddr);
        if (toAccount != nullptr)
        {
            if (toAccount->isContract())
            {
                LOG_GENERAL(WARNING,
                            "Contract account won't accept normal transaction");
                return false;
            }
        }

        return AccountStoreBase<MAP>::UpdateAccounts(transaction, receipt);
    }

    bool callContract = false;

    if (transaction.GetData().size() > 0 && toAddr != NullAddress
        && transaction.GetCode().empty())
    {
        callContract = true;
    }

    // Needed by gas handling
    bool validToTransferBalance = true;

    Account* fromAccount = this->GetAccount(fromAddr);
    if (fromAccount == nullptr)
    {
        // FIXME: remove this, temporary way to test transactions, should return false
        LOG_GENERAL(WARNING,
                    "AddAccount... FIXME: remove this, temporary way to "
                    "test transactions, should return false in the future");
        this->AddAccount(fromAddr, {10000000000, 0});
        fromAccount = this->GetAccount(fromAddr);
        // return false;
    }

    if (transaction.GetCode().size() > 0)
    {
        if (toAddr != NullAddress)
        {
            LOG_GENERAL(WARNING,
                        "txn has non-empty code but with valid toAddr");
            return false;
        }

        LOG_GENERAL(INFO, "Create Contract");

        if (transaction.GetGasLimit() < CONTRACT_CREATE_GAS)
        {
            LOG_GENERAL(
                WARNING,
                "The gas limit set for this transaction has to be larger than"
                " the gas to create a contract ("
                    << CONTRACT_CREATE_GAS << ")");
            return false;
        }

        if (fromAccount->GetBalance() < gasDeposit)
        {
            LOG_GENERAL(
                WARNING,
                "The account doesn't have enough gas to create a contract");
            return false;
        }
        else if (fromAccount->GetBalance() < gasDeposit + amount)
        {
            LOG_GENERAL(WARNING,
                        "The account (balance: "
                            << fromAccount->GetBalance()
                            << ") "
                               "has enough balance to pay the gas limit ("
                            << gasDeposit
                            << ") "
                               "but not enough for transfer the amount ("
                            << amount
                            << "), "
                               "create contract first and ignore amount "
                               "transfer however");
            validToTransferBalance = false;
        }

        if (!this->DecreaseBalance(fromAddr, gasDeposit))
        {
            return false;
        }

        toAddr
            = Account::GetAddressForContract(fromAddr, fromAccount->GetNonce());
        this->AddAccount(toAddr, {0, 0});
        Account* toAccount = this->GetAccount(toAddr);
        toAccount->SetCode(transaction.GetCode());
        // Store the immutable states
        toAccount->InitContract(transaction.GetData());
        // Set the blockNumber when the account was created
        toAccount->SetCreateBlockNum(blockNum);

        m_curBlockNum = blockNum;

        ExportCreateContractFiles(*toAccount);

        bool ret = true;
        if (!SysCommand::ExecuteCmdWithoutOutput(
                GetCreateContractCmdStr(gasRemained)))
        {
            ret = false;
        }
        if (ret && !ParseCreateContractOutput(gasRemained))
        {
            ret = false;
        }
        if (!ret)
        {
            gasRemained = std::min(
                transaction.GetGasLimit() - CONTRACT_CREATE_GAS, gasRemained);
        }
        uint256_t gasRefund;
        if (!SafeMath<uint256_t>::mul(gasRemained, transaction.GetGasPrice(),
                                      gasRefund))
        {
            this->m_addressToAccount->erase(toAddr);
            return false;
        }
        this->IncreaseBalance(fromAddr, gasRefund);
        if (!ret)
        {
            this->m_addressToAccount->erase(toAddr);

            receipt.SetResult(false);
            receipt.SetCumGas(CONTRACT_CREATE_GAS);
            receipt.update();

            return true; // Return true because the states already changed
        }
    }

    if (!callContract)
    {
        if (validToTransferBalance)
        {
            if (!this->TransferBalance(fromAddr, toAddr, amount))
            {
                this->IncreaseNonce(fromAddr);
                receipt.SetResult(false);
                receipt.SetCumGas(transaction.GetGasLimit() - gasRemained);
                receipt.update();

                return true;
            }
        }

        receipt.SetCumGas(transaction.GetGasLimit() - gasRemained);
    }
    else
    {
        LOG_GENERAL(INFO, "Call Contract");

        if (transaction.GetGasLimit() < CONTRACT_INVOKE_GAS)
        {
            LOG_GENERAL(
                WARNING,
                "The gas limit set for this transaction has to be larger than"
                " the minimum gas to invoke contract ("
                    << CONTRACT_INVOKE_GAS << ")");
            return false;
        }

        if (fromAccount->GetBalance() < gasDeposit + amount)
        {
            LOG_GENERAL(
                WARNING,
                "The account (balance: "
                    << fromAccount->GetBalance()
                    << ") "
                       "has not enough balance to deposit the gas limit ("
                    << gasDeposit
                    << ") "
                       "and transfer the amount ("
                    << amount
                    << ") in the transaction, "
                       "rejected");
            return false;
        }

        m_curSenderAddr = fromAddr;

        Account* toAccount = this->GetAccount(toAddr);
        if (toAccount == nullptr)
        {
            LOG_GENERAL(WARNING, "The target contract account doesn't exist");
            return false;
        }

        m_curBlockNum = blockNum;
        if (!ExportCallContractFiles(*toAccount, transaction))
        {
            return false;
        }

        DiscardTransferBalanceAtomic();

        if (!this->DecreaseBalance(fromAddr, gasDeposit))
        {
            return false;
        }
        m_curGasLimit = transaction.GetGasLimit();
        m_curGasPrice = transaction.GetGasPrice();

        m_curContractAddr = toAddr;
        m_curAmount = amount;
        m_curNumShards = numShards;
        m_curTranReceipt.clear();

        // if (!TransferBalanceAtomic(fromAddr, toAddr, amount))
        // {
        //     this->IncreaseBalance(fromAddr, gasDeposit);
        //     return false;
        // }
        bool ret = true;
        if (!SysCommand::ExecuteCmdWithoutOutput(
                GetCallContractCmdStr(gasRemained)))
        {
            ret = false;
        }

        if (ret && !ParseCallContractOutput(gasRemained))
        {
            ret = false;
        }
        if (!ret)
        {
            DiscardTransferBalanceAtomic();
            gasRemained = std::min(
                transaction.GetGasLimit() - CONTRACT_INVOKE_GAS, gasRemained);
        }
        else
        {
            CommitTransferBalanceAtomic();
        }
        uint256_t gasRefund;
        if (!SafeMath<uint256_t>::mul(gasRemained, transaction.GetGasPrice(),
                                      gasRefund))
        {
            return false;
        }

        this->IncreaseBalance(fromAddr, gasRefund);
        receipt = m_curTranReceipt;
        receipt.SetCumGas(transaction.GetGasLimit() - gasRemained);
        if (!ret)
        {
            receipt.SetResult(false);
            receipt.update();
            return true; // Return true because the states already changed
        }
    }

    this->IncreaseNonce(fromAddr);

    receipt.SetResult(true);
    receipt.update();

    return true;
}

template<class MAP>
Json::Value
AccountStoreSC<MAP>::GetBlockStateJson(const uint64_t& BlockNum) const
{
    Json::Value root;
    Json::Value blockItem;
    blockItem["vname"] = "BLOCKNUMBER";
    blockItem["type"] = "BNum";
    blockItem["value"] = std::to_string(BlockNum);
    root.append(blockItem);

    return root;
}

template<class MAP>
void AccountStoreSC<MAP>::ExportCreateContractFiles(const Account& contract)
{
    LOG_MARKER();

    boost::filesystem::remove_all("./" + SCILLA_FILES);
    boost::filesystem::create_directories("./" + SCILLA_FILES);

    if (!(boost::filesystem::exists("./" + SCILLA_LOG)))
    {
        boost::filesystem::create_directories("./" + SCILLA_LOG);
    }

    // Scilla code
    // JSONUtils::writeJsontoFile(INPUT_CODE, contract.GetCode());
    std::ofstream os(INPUT_CODE);
    os << DataConversion::CharArrayToString(contract.GetCode());
    os.close();

    // Initialize Json
    JSONUtils::writeJsontoFile(INIT_JSON, contract.GetInitJson());

    // Block Json
    JSONUtils::writeJsontoFile(INPUT_BLOCKCHAIN_JSON,
                               GetBlockStateJson(m_curBlockNum));
}

template<class MAP>
void AccountStoreSC<MAP>::ExportContractFiles(const Account& contract)
{
    LOG_MARKER();

    boost::filesystem::remove_all("./" + SCILLA_FILES);
    boost::filesystem::create_directories("./" + SCILLA_FILES);

    if (!(boost::filesystem::exists("./" + SCILLA_LOG)))
    {
        boost::filesystem::create_directories("./" + SCILLA_LOG);
    }

    // Scilla code
    // JSONUtils::writeJsontoFile(INPUT_CODE, contract.GetCode());
    std::ofstream os(INPUT_CODE);
    os << DataConversion::CharArrayToString(contract.GetCode());
    os.close();

    // Initialize Json
    JSONUtils::writeJsontoFile(INIT_JSON, contract.GetInitJson());

    // State Json
    JSONUtils::writeJsontoFile(INPUT_STATE_JSON, contract.GetStorageJson());

    // Block Json
    JSONUtils::writeJsontoFile(INPUT_BLOCKCHAIN_JSON,
                               GetBlockStateJson(m_curBlockNum));
}

template<class MAP>
bool AccountStoreSC<MAP>::ExportCallContractFiles(
    const Account& contract, const Transaction& transaction)
{
    LOG_MARKER();

    ExportContractFiles(contract);

    // Message Json
    std::string dataStr(transaction.GetData().begin(),
                        transaction.GetData().end());
    Json::Value msgObj;
    if (!JSONUtils::convertStrtoJson(dataStr, msgObj))
    {
        return false;
    }
    std::string prepend = "0x";
    msgObj["_sender"] = prepend
        + Account::GetAddressFromPublicKey(transaction.GetSenderPubKey()).hex();
    msgObj["_amount"] = transaction.GetAmount().convert_to<std::string>();

    JSONUtils::writeJsontoFile(INPUT_MESSAGE_JSON, msgObj);

    return true;
}

template<class MAP>
void AccountStoreSC<MAP>::ExportCallContractFiles(
    const Account& contract, const Json::Value& contractData)
{
    LOG_MARKER();

    ExportContractFiles(contract);

    JSONUtils::writeJsontoFile(INPUT_MESSAGE_JSON, contractData);
}

<<<<<<< HEAD
template<class MAP>
string
AccountStoreSC<MAP>::GetCreateContractCmdStr(const uint256_t& available_gas)
=======
template<class MAP> std::string AccountStoreSC<MAP>::GetCreateContractCmdStr()
>>>>>>> 77c987635d7e82137a3f851e09c492b22876b469
{
    std::string ret = SCILLA_BINARY + " -init " + INIT_JSON + " -iblockchain "
        + INPUT_BLOCKCHAIN_JSON + " -o " + OUTPUT_JSON + " -i " + INPUT_CODE
        + " -libdir " + SCILLA_LIB + " -gaslimit "
        + available_gas.convert_to<string>();
    LOG_GENERAL(INFO, ret);
    return ret;
}

<<<<<<< HEAD
template<class MAP>
string
AccountStoreSC<MAP>::GetCallContractCmdStr(const uint256_t& available_gas)
=======
template<class MAP> std::string AccountStoreSC<MAP>::GetCallContractCmdStr()
>>>>>>> 77c987635d7e82137a3f851e09c492b22876b469
{
    std::string ret = SCILLA_BINARY + " -init " + INIT_JSON + " -istate "
        + INPUT_STATE_JSON + " -iblockchain " + INPUT_BLOCKCHAIN_JSON
        + " -imessage " + INPUT_MESSAGE_JSON + " -o " + OUTPUT_JSON + " -i "
        + INPUT_CODE + " -libdir " + SCILLA_LIB + " -gaslimit "
        + available_gas.convert_to<string>();
    LOG_GENERAL(INFO, ret);
    return ret;
}

template<class MAP>
bool AccountStoreSC<MAP>::ParseCreateContractOutput(uint256_t& gasRemained)
{
    // LOG_MARKER();

    std::ifstream in(OUTPUT_JSON, std::ios::binary);

    if (!in.is_open())
    {
        LOG_GENERAL(WARNING,
                    "Error opening output file or no output file generated");
        return false;
    }
    std::string outStr{std::istreambuf_iterator<char>(in),
                       std::istreambuf_iterator<char>()};
    LOG_GENERAL(INFO, "Output: " << std::endl << outStr);
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    std::string errors;
    if (reader->parse(outStr.c_str(), outStr.c_str() + outStr.size(), &root,
                      &errors))
    {
        return ParseCreateContractJsonOutput(root, gasRemained);
    }
    else
    {
        LOG_GENERAL(WARNING,
                    "Failed to parse contract output json: " << errors);
        return false;
    }
}

template<class MAP>
bool AccountStoreSC<MAP>::ParseCreateContractJsonOutput(
    const Json::Value& _json, uint256_t& gasRemained)
{
    // LOG_MARKER();
    if (!_json.isMember("remaining_gas"))
    {
        LOG_GENERAL(
            WARNING,
            "The json output of this contract didn't contain remaining_gas");
        if (gasRemained > CONTRACT_CREATE_GAS)
        {
            gasRemained -= CONTRACT_CREATE_GAS;
        }
        else
        {
            gasRemained = 0;
        }
        return false;
    }
    gasRemained = atoi(_json["remaining_gas"].asString().c_str());

    if (!_json.isMember("message") || !_json.isMember("states"))
    {
        LOG_GENERAL(WARNING,
                    "The json output of this contract is corrupted or create "
                    "contract failed");
        return false;
    }

    if (_json["message"] == Json::nullValue
        && _json["states"] == Json::arrayValue)
    {
        // LOG_GENERAL(INFO, "Get desired json output from the interpreter for create contract");
        return true;
    }
    LOG_GENERAL(WARNING,
                "Didn't get desired json output from the interpreter for "
                "create contract");
    return false;
}

template<class MAP>
bool AccountStoreSC<MAP>::ParseCallContractOutput(uint256_t& gasRemained)
{
    // LOG_MARKER();

    std::ifstream in(OUTPUT_JSON, std::ios::binary);

    if (!in.is_open())
    {
        LOG_GENERAL(WARNING,
                    "Error opening output file or no output file generated");
        return false;
    }
    std::string outStr{std::istreambuf_iterator<char>(in),
                       std::istreambuf_iterator<char>()};
    LOG_GENERAL(INFO, "Output: " << std::endl << outStr);
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    std::string errors;
    if (reader->parse(outStr.c_str(), outStr.c_str() + outStr.size(), &root,
                      &errors))
    {
        return ParseCallContractJsonOutput(root, gasRemained);
    }
    else
    {
        LOG_GENERAL(WARNING,
                    "Failed to parse contract output json: " << errors);
        return false;
    }
}

template<class MAP>
bool AccountStoreSC<MAP>::ParseCallContractJsonOutput(const Json::Value& _json,
                                                      uint256_t& gasRemained)
{
    // LOG_MARKER();
    if (!_json.isMember("remaining_gas"))
    {
        LOG_GENERAL(
            WARNING,
            "The json output of this contract didn't contain remaining_gas");
        if (gasRemained > CONTRACT_INVOKE_GAS)
        {
            gasRemained -= CONTRACT_INVOKE_GAS;
        }
        else
        {
            gasRemained = 0;
        }
        return false;
    }
    gasRemained = atoi(_json["remaining_gas"].asString().c_str());

    if (!_json.isMember("message") || !_json.isMember("states")
        || !_json.isMember("events"))
    {
        LOG_GENERAL(WARNING,
                    "The json output of this contract is corrupted or call "
                    "transition failed");
        return false;
    }

    if (!_json["message"].isMember("_tag")
        || !_json["message"].isMember("_amount")
        || !_json["message"].isMember("params")
        || !_json["message"].isMember("_recipient")
        || !_json["message"].isMember("_accepted"))
    {
        LOG_GENERAL(
            WARNING,
            "The message in the json output of this contract is corrupted");
        return false;
    }

    if (_json["message"]["_accepted"].asString() == "true")
    {
        // LOG_GENERAL(INFO, "Contract accept amount transfer");
        if (!TransferBalanceAtomic(m_curSenderAddr, m_curContractAddr,
                                   m_curAmount))
        {
            LOG_GENERAL(WARNING, "TransferBalance Atomic failed");
            return false;
        }
    }
    else
    {
        LOG_GENERAL(WARNING, "Contract refuse amount transfer");
    }

    for (const auto& s : _json["states"])
    {
        if (!s.isMember("vname") || !s.isMember("type") || !s.isMember("value"))
        {
            LOG_GENERAL(WARNING,
                        "Address: "
                            << m_curContractAddr.hex()
                            << ", The json output of states is corrupted");
            continue;
        }
        std::string vname = s["vname"].asString();
        std::string type = s["type"].asString();
        std::string value = s["value"].isString()
            ? s["value"].asString()
            : JSONUtils::convertJsontoStr(s["value"]);

        Account* contractAccount = this->GetAccount(m_curContractAddr);
        if (vname != "_balance")
        {
            contractAccount->SetStorage(vname, type, value);
        }
    }

    for (const auto& e : _json["events"])
    {
        LogEntry entry;
        if (!entry.Install(e, m_curContractAddr))
        {
            return false;
        }
        m_curTranReceipt.AddEntry(entry);
    }

    Address recipient = Address(_json["message"]["_recipient"].asString());
    Account* account = this->GetAccount(recipient);

    if (account == nullptr)
    {
        LOG_GENERAL(WARNING, "The recipient account doesn't exist");
        return false;
    }

    // Recipient is non-contract
    if (!account->isContract())
    {
        LOG_GENERAL(INFO, "The recipient is non-contract");
        return TransferBalanceAtomic(
            m_curContractAddr, recipient,
            atoi(_json["message"]["_amount"].asString().c_str()));
    }

    // Recipient is contract
    // _tag field is empty
    if (_json["message"]["_tag"].asString().empty())
    {
        LOG_GENERAL(INFO,
                    "_tag in the scilla output is empty when invoking a "
                    "contract, transaction finished");
        return true;
    }

    LOG_GENERAL(INFO, "Call another contract");

    // check whether the recipient contract is in the same shard with the current contract
    if (!m_curIsDS
        && Transaction::GetShardIndex(m_curContractAddr, m_curNumShards)
            != Transaction::GetShardIndex(recipient, m_curNumShards))
    {
        LOG_GENERAL(WARNING,
                    "another contract doesn't belong to the same shard with "
                    "current contract");
        return false;
    }

    Json::Value input_message;
    input_message["_sender"] = "0x" + m_curContractAddr.hex();
    input_message["_amount"] = _json["message"]["_amount"];
    input_message["_tag"] = _json["message"]["_tag"];
    input_message["params"] = _json["message"]["params"];

    ExportCallContractFiles(*account, input_message);

    if (!TransferBalanceAtomic(
            m_curContractAddr, recipient,
            atoi(_json["message"]["_amount"].asString().c_str())))
    {
        return false;
    }

    if (!SysCommand::ExecuteCmdWithoutOutput(
            GetCallContractCmdStr(gasRemained)))
    {
        LOG_GENERAL(
            WARNING,
            "ExecuteCmd failed: " << GetCallContractCmdStr(gasRemained));
        return false;
    }
    Address t_address = m_curContractAddr;
    m_curContractAddr = recipient;
    if (!ParseCallContractOutput(gasRemained))
    {
        LOG_GENERAL(WARNING,
                    "ParseCallContractOutput failed of calling contract: "
                        << recipient);
        return false;
    }
    this->IncreaseNonce(t_address);
    return true;
}

template<class MAP>
bool AccountStoreSC<MAP>::TransferBalanceAtomic(
    const Address& from, const Address& to,
    const boost::multiprecision::uint256_t& delta)
{
    // LOG_MARKER();
    return m_accountStoreAtomic->TransferBalance(from, to, delta);
}

template<class MAP> void AccountStoreSC<MAP>::CommitTransferBalanceAtomic()
{
    LOG_MARKER();
    for (const auto& entry : *m_accountStoreAtomic->GetAddressToAccount())
    {
        Account* account = this->GetAccount(entry.first);
        if (account != nullptr)
        {
            account->SetBalance(entry.second.GetBalance());
        }
        else
        {
            // this->m_addressToAccount.emplace(std::make_pair(entry.first, entry.second));
            this->AddAccount(entry.first, entry.second);
        }
    }
}

template<class MAP> void AccountStoreSC<MAP>::DiscardTransferBalanceAtomic()
{
    LOG_MARKER();
    m_accountStoreAtomic->Init();
}