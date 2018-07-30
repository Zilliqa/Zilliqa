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
    m_accountStoreAtomic = make_unique<AccountStoreAtomic<MAP>>(*this);
}

template<class MAP> void AccountStoreSC<MAP>::Init()
{
    lock_guard<mutex> g(m_mutexUpdateAccounts);
    AccountStoreBase<MAP>::Init();
    m_curContractAddr.clear();
    m_curSenderAddr.clear();
    m_curAmount = 0;
    m_curGasCum = 0;
    m_curGasLimit = 0;
    m_curGasPrice = 0;
}

template<class MAP>
bool AccountStoreSC<MAP>::UpdateAccounts(const uint64_t& blockNum,
                                         const Transaction& transaction)
{
    // LOG_MARKER();

    lock_guard<mutex> g(m_mutexUpdateAccounts);

    const PubKey& senderPubKey = transaction.GetSenderPubKey();
    const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
    Address toAddr = transaction.GetToAddr();

    const uint256_t& amount = transaction.GetAmount();

    // FIXME: Possible integer overflow here
    uint256_t gasDeposit;
    if (!SafeMath::mul(transaction.GetGasLimit(), transaction.GetGasPrice(),
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

        return AccountStoreBase<MAP>::UpdateAccounts(transaction);
    }

    bool callContract = false;

    if (transaction.GetData().size() > 0 && toAddr != NullAddress)
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

    if (transaction.GetCode().size() > 0 && toAddr == NullAddress)
    {
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
        if (!SysCommand::ExecuteCmdWithoutOutput(GetCreateContractCmdStr()))
        {
            ret = false;
        }
        if (ret && !ParseCreateContractOutput())
        {
            ret = false;
        }
        uint256_t gasRefund;
        if (!AccountStoreBase<MAP>::CalculateGasRefund(
                gasDeposit, CONTRACT_CREATE_GAS, transaction.GetGasPrice(),
                gasRefund))
        {
            this->m_addressToAccount->erase(toAddr);
            return false;
        }
        this->IncreaseBalance(fromAddr, gasRefund);
        if (!ret)
        {
            this->m_addressToAccount->erase(toAddr);
            return true; // Return true because the states already changed
        }
    }

    if (!callContract && validToTransferBalance)
    {
        if (!this->TransferBalance(fromAddr, toAddr, amount))
        {
            this->IncreaseNonce(fromAddr);
            return false;
        }
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
        m_curGasCum = CalculateGas();
        m_curGasPrice = transaction.GetGasPrice();

        m_curContractAddr = toAddr;
        m_curAmount = amount;

        // if (!TransferBalanceAtomic(fromAddr, toAddr, amount))
        // {
        //     this->IncreaseBalance(fromAddr, gasDeposit);
        //     return false;
        // }
        bool ret = true;
        if (!SysCommand::ExecuteCmdWithoutOutput(GetCallContractCmdStr()))
        {
            ret = false;
        }
        if (ret && !ParseCallContractOutput())
        {
            ret = false;
        }
        if (!ret)
        {
            DiscardTransferBalanceAtomic();
        }
        else
        {
            CommitTransferBalanceAtomic();
        }

        uint256_t gasRefund;
        if (!AccountStoreBase<MAP>::CalculateGasRefund(
                gasDeposit, m_curGasCum, m_curGasPrice, gasRefund))
        {
            return false;
        }
        this->IncreaseBalance(fromAddr, gasRefund);
        if (!ret)
        {
            return true; // Return true because the states already changed
        }
    }

    this->IncreaseNonce(fromAddr);

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
    blockItem["value"] = to_string(BlockNum);
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
    string dataStr(transaction.GetData().begin(), transaction.GetData().end());
    Json::Value msgObj;
    if (!JSONUtils::convertStrtoJson(dataStr, msgObj))
    {
        return false;
    }
    string prepend = "0x";
    msgObj["_sender"] = prepend
        + Account::GetAddressFromPublicKey(transaction.GetSenderPubKey()).hex();
    msgObj["_amount"] = transaction.GetAmount().convert_to<string>();

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

template<class MAP> string AccountStoreSC<MAP>::GetCreateContractCmdStr()
{
    string ret = SCILLA_BINARY + " -init " + INIT_JSON + " -iblockchain "
        + INPUT_BLOCKCHAIN_JSON + " -o " + OUTPUT_JSON + " -i " + INPUT_CODE
        + " -libdir " + SCILLA_LIB;
    LOG_GENERAL(INFO, ret);
    return ret;
}

template<class MAP> string AccountStoreSC<MAP>::GetCallContractCmdStr()
{
    string ret = SCILLA_BINARY + " -init " + INIT_JSON + " -istate "
        + INPUT_STATE_JSON + " -iblockchain " + INPUT_BLOCKCHAIN_JSON
        + " -imessage " + INPUT_MESSAGE_JSON + " -o " + OUTPUT_JSON + " -i "
        + INPUT_CODE + " -libdir " + SCILLA_LIB;
    LOG_GENERAL(INFO, ret);
    return ret;
}

template<class MAP> bool AccountStoreSC<MAP>::ParseCreateContractOutput()
{
    // LOG_MARKER();

    ifstream in(OUTPUT_JSON, ios::binary);

    if (!in.is_open())
    {
        LOG_GENERAL(WARNING,
                    "Error opening output file or no output file generated");
        return false;
    }
    string outStr{istreambuf_iterator<char>(in), istreambuf_iterator<char>()};
    LOG_GENERAL(INFO, "Output: " << endl << outStr);
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    string errors;
    if (reader->parse(outStr.c_str(), outStr.c_str() + outStr.size(), &root,
                      &errors))
    {
        return ParseCreateContractJsonOutput(root);
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
    const Json::Value& _json)
{
    // LOG_MARKER();

    if (!_json.isMember("message") || !_json.isMember("states"))
    {
        LOG_GENERAL(WARNING, "The json output of this contract is corrupted");
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

template<class MAP> bool AccountStoreSC<MAP>::ParseCallContractOutput()
{
    // LOG_MARKER();

    ifstream in(OUTPUT_JSON, ios::binary);

    if (!in.is_open())
    {
        LOG_GENERAL(WARNING,
                    "Error opening output file or no output file generated");
        return false;
    }
    string outStr{istreambuf_iterator<char>(in), istreambuf_iterator<char>()};
    LOG_GENERAL(INFO, "Output: " << endl << outStr);
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    string errors;
    if (reader->parse(outStr.c_str(), outStr.c_str() + outStr.size(), &root,
                      &errors))
    {
        return ParseCallContractJsonOutput(root);
    }
    else
    {
        LOG_GENERAL(WARNING,
                    "Failed to parse contract output json: " << errors);
        return false;
    }
}

template<class MAP>
bool AccountStoreSC<MAP>::ParseCallContractJsonOutput(const Json::Value& _json)
{
    // LOG_MARKER();

    if (!_json.isMember("message") || !_json.isMember("states"))
    {
        LOG_GENERAL(WARNING, "The json output of this contract is corrupted");
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

    for (auto s : _json["states"])
    {
        if (!s.isMember("vname") || !s.isMember("type") || !s.isMember("value"))
        {
            LOG_GENERAL(WARNING,
                        "Address: "
                            << m_curContractAddr.hex()
                            << ", The json output of states is corrupted");
            continue;
        }
        string vname = s["vname"].asString();
        string type = s["type"].asString();
        string value = s["value"].isString()
            ? s["value"].asString()
            : JSONUtils::convertJsontoStr(s["value"]);

        Account* contractAccount = this->GetAccount(m_curContractAddr);
        if (vname != "_balance")
        {
            contractAccount->SetStorage(vname, type, value);
        }
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

    if (CheckGasExceededLimit(CalculateGas()))
    {
        LOG_GENERAL(
            WARNING,
            "The predicted accumulated gas has already exceed the gas limit,"
            "can no longer continue the invocation");
        return false;
    }
    m_curGasCum += CalculateGas();

    if (!SysCommand::ExecuteCmdWithoutOutput(GetCallContractCmdStr()))
    {
        LOG_GENERAL(WARNING, "ExecuteCmd failed: " << GetCallContractCmdStr());
        return false;
    }
    Address t_address = m_curContractAddr;
    m_curContractAddr = recipient;
    if (!ParseCallContractOutput())
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
bool AccountStoreSC<MAP>::TransferBalanceAtomic(const Address& from,
                                                const Address& to,
                                                const uint256_t& delta)
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
            // this->m_addressToAccount.emplace(make_pair(entry.first, entry.second));
            this->AddAccount(entry.first, entry.second);
        }
    }
}

template<class MAP> void AccountStoreSC<MAP>::DiscardTransferBalanceAtomic()
{
    LOG_MARKER();
    m_accountStoreAtomic->Init();
}

template<class MAP> uint256_t AccountStoreSC<MAP>::CalculateGas()
{
    // TODO: return gas based on calculation of customed situations
    return uint256_t(CONTRACT_INVOKE_GAS);
}

template<class MAP>
bool AccountStoreSC<MAP>::CheckGasExceededLimit(const uint256_t& gas)
{
    return m_curGasCum + gas > m_curGasLimit;
}
