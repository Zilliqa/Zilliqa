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
#include "libUtils/SysCommand.h"

template<class MAP>
AccountStoreSC<MAP>::AccountStoreSC()
{
}

template<class MAP> void AccountStoreSC<MAP>::Init()
{
    AccountStoreBase<MAP>::Init();
    m_curContractAddr.clear();
}

template<class MAP>
bool AccountStoreSC<MAP>::UpdateAccounts(const uint64_t& blockNum,
                                          const Transaction& transaction)
{
    LOG_MARKER();

    const PubKey& senderPubKey = transaction.GetSenderPubKey();
    const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
    Address toAddr = transaction.GetToAddr();
    const uint256_t& amount = transaction.GetAmount();

    bool callContract = false;

    if (transaction.GetData().size() > 0 && toAddr != NullAddress)
    {
        // if (amount == 0)
        // {
        //     LOG_GENERAL(
        //         WARNING,
        //         "The amount for calling a contract shouldn't be zero");
        //     return false;
        // }
        callContract = true;
    }

    if (transaction.GetCode().size() > 0 && toAddr == NullAddress)
    {
        LOG_GENERAL(INFO, "Create Contract");

        if (amount != 0)
        {
            LOG_GENERAL(
                WARNING,
                "The amount for creating a contract should be zero");
            return false;
        }

        // Create contract account
        Account* fromAccount = this->GetAccount(fromAddr);
        if (fromAccount == nullptr)
        {
	        // FIXME: remove this, temporary way to test transactions, should return false
            LOG_GENERAL(
                WARNING,
                "AddAccount... FIXME: remove this, temporary way to "
                "test transactions, should return false in the future");
            this->AddAccount(fromAddr, {10000000000, 0});
            fromAccount = this->GetAccount(fromAddr);
        }

        toAddr
            = Account::GetAddressForContract(fromAddr, fromAccount->GetNonce());
        this->AddAccount(toAddr, {0, 0});
        Account* toAccount = this->GetAccount(toAddr);
        toAccount->SetCode(transaction.GetCode());
        // Store the immutable states
        toAccount->InitContract(transaction.GetData());

        m_curBlockNum = blockNum;

        ExportCreateContractFiles(*toAccount);
        if (!SysCommand::ExecuteCmdWithoutOutput(GetCreateContractCmdStr()))
        {
            return false;
        }
        if (!ParseCreateContractOutput())
        {
            this->m_addressToAccount->erase(toAddr);
            return false;
        }
    }

    if (!callContract)
    {
        if (!this->TransferBalance(fromAddr, toAddr, amount))
        {
            return false;
        }
    }
    else
    {
        LOG_GENERAL(INFO, "Call Contract");

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
        m_curContractAddr = toAddr;

        //clean-up
        DiscardTransferBalanceAtomic();
        if (!TransferBalanceAtomic(fromAddr, toAddr, amount))
        {
            return false;
        }

        if (!SysCommand::ExecuteCmdWithoutOutput(GetCallContractCmdStr()))
        {
            DiscardTransferBalanceAtomic();
            return false;
        }
        if (!ParseCallContractOutput())
        {
            DiscardTransferBalanceAtomic();
            return false;
        }
        CommitTransferBalanceAtomic();
    }

    this->IncreaseNonce(fromAddr);

    return true;
}

template<class MAP>
Json::Value AccountStoreSC<MAP>::GetBlockStateJson(const uint64_t& BlockNum) const
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
    std::ofstream os;
    os.open(INPUT_CODE);
    os << DataConversion::CharArrayToString(contract.GetCode());
    os.close();

    // Initialize Json
    JSONUtils::writeJsontoFile(INIT_JSON, contract.GetInitJson());

    // Block Json
    JSONUtils::writeJsontoFile(INPUT_BLOCKCHAIN_JSON, GetBlockStateJson(m_curBlockNum));
}

template<class MAP>
void AccountStoreSC<MAP>::ExportContractFiles(const Account& contract)
{
    LOG_MARKER();

    boost::filesystem::remove_all("./" + SCILLA_FILES);
    boost::filesystem::create_directories("./" + SCILLA_FILES);

    // Scilla code
    // JSONUtils::writeJsontoFile(INPUT_CODE, contract.GetCode());
    std::ofstream os;
    os.open(INPUT_CODE);
    os << DataConversion::CharArrayToString(contract.GetCode());
    os.close();

    // Initialize Json
    JSONUtils::writeJsontoFile(INIT_JSON, contract.GetInitJson());

    // State Json
    JSONUtils::writeJsontoFile(INPUT_STATE_JSON, contract.GetStorageJson());

    // Block Json
    JSONUtils::writeJsontoFile(INPUT_BLOCKCHAIN_JSON, GetBlockStateJson(m_curBlockNum));
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
        boost::filesystem::remove_all("./" + SCILLA_FILES);
        return false;
    }
    string prepend = "0x";
    msgObj["_sender"] = prepend + Account::GetAddressFromPublicKey(transaction.GetSenderPubKey()).hex();
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
    string ret = SCILLA_PATH + " -init " + INIT_JSON + " -iblockchain "
        + INPUT_BLOCKCHAIN_JSON + " -o " + OUTPUT_JSON + " -i " + INPUT_CODE;
    LOG_GENERAL(INFO, ret);
    return ret;
}

template<class MAP> string AccountStoreSC<MAP>::GetCallContractCmdStr()
{
    string ret = SCILLA_PATH + " -init " + INIT_JSON + " -istate "
        + INPUT_STATE_JSON + " -iblockchain " + INPUT_BLOCKCHAIN_JSON
        + " -imessage " + INPUT_MESSAGE_JSON + " -o " + OUTPUT_JSON + " -i "
        + INPUT_CODE;
    LOG_GENERAL(INFO, ret);
    return ret;
}

template<class MAP> bool AccountStoreSC<MAP>::ParseCreateContractOutput()
{
    LOG_MARKER();

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
    LOG_MARKER();

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
    LOG_MARKER();

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
    LOG_MARKER();

    if (!_json.isMember("message") || !_json.isMember("states"))
    {
        LOG_GENERAL(WARNING, "The json output of this contract is corrupted");
        return false;
    }

    if (!_json["message"].isMember("_tag")
        || !_json["message"].isMember("_amount")
        || !_json["message"].isMember("params")
        || !_json["message"].isMember("_recipient"))
    {
        LOG_GENERAL(
            WARNING,
            "The message in the json output of this contract is corrupted");
        return false;
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
        string value;
        if (type == "Map" || type == "ADT")
        {
            value = JSONUtils::convertJsontoStr(s["value"]);
        }
        else
        {
            value = s["value"].asString();
        }

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
        return TransferBalanceAtomic(m_curContractAddr, recipient, atoi(_json["message"]["_amount"].asString().c_str()));
    }

    // Recipient is contract
    // _tag field is empty
    if (_json["message"]["_tag"].asString().empty())
    {
        LOG_GENERAL(WARNING, "_tag in the scilla output is empty when invoking a contract");
        return false;
    }

    LOG_GENERAL(INFO, "Call another contract");

    Json::Value input_message;
    input_message["_sender"] = "0x" + m_curContractAddr.hex();
    input_message["_amount"] = _json["message"]["_amount"];
    input_message["_tag"] = _json["message"]["_tag"];
    input_message["params"] = _json["message"]["params"];

    ExportCallContractFiles(*account, input_message);

    if (!TransferBalanceAtomic(m_curContractAddr, recipient, atoi(_json["message"]["_amount"].asString().c_str())))
    {
        return false;
    }
    
    if (!SysCommand::ExecuteCmdWithoutOutput(GetCallContractCmdStr()))
    {
        LOG_GENERAL(WARNING, "ExecuteCmd failed: " << GetCallContractCmdStr());
        return false;
    }
    Address t_address = m_curContractAddr;
    m_curContractAddr = recipient;
    if (!ParseCallContractOutput())
    {
        LOG_GENERAL(WARNING, "ParseCallContractOutput failed of calling contract: " << recipient);
        return false;
    }
    this->IncreaseNonce(t_address);
    return true;
}

template<class MAP>  
bool AccountStoreSC<MAP>::TransferBalanceAtomic(const Address& from, const Address& to,
                                                    const uint256_t& delta)
{
    // if (DecreaseBalanceAtomic(from, delta) && IncreaseBalanceAtomic(to, delta))
    // {
    //     return true;
    // }
    return false;
}

template<class MAP>
void AccountStoreSC<MAP>::CommitTransferBalanceAtomic()
{

}

template<class MAP>
void AccountStoreSC<MAP>::DiscardTransferBalanceAtomic()
{

}