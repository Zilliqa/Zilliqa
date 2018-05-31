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
#include <type_traits>

// #include "AccountStoreBase.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include "libUtils/SysCommand.h"

template<class DB>
AccountStoreBase<DB>::AccountStoreBase()
    : m_db(is_same<DB, OverlayDB>::value ? "state" : NULL)
{
    m_addressToAccount = make_shared<unordered_map<Address, Account>>();
}

template<class DB> void AccountStoreBase<DB>::Init()
{
    LOG_MARKER();
    m_addressToAccount->clear();
    m_state.init();
    prevRoot = m_state.root();
}

template<class DB>
unsigned int AccountStoreBase<DB>::Serialize(vector<unsigned char>& dst,
                                             unsigned int offset) const
{
    // [Total number of accounts (uint256_t)] [Addr 1] [Account 1] [Addr 2] [Account 2] .... [Addr n] [Account n]

    // LOG_MARKER();

    unsigned int size_needed = UINT256_SIZE;
    unsigned int size_remaining = dst.size() - offset;
    unsigned int totalSerializedSize = size_needed;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    unsigned int curOffset = offset;

    // [Total number of accounts]
    LOG_GENERAL(
        INFO,
        "Debug: Total number of accounts to serialize: " << GetNumOfAccounts());
    uint256_t totalNumOfAccounts = GetNumOfAccounts();
    SetNumber<uint256_t>(dst, curOffset, totalNumOfAccounts, UINT256_SIZE);
    curOffset += UINT256_SIZE;

    vector<unsigned char> address_vec;
    // [Addr 1] [Account 1] [Addr 2] [Account 2] .... [Addr n] [Account n]
    for (auto entry : *m_addressToAccount)
    {
        // Address
        address_vec = entry.first.asBytes();

        copy(address_vec.begin(), address_vec.end(), std::back_inserter(dst));
        curOffset += ACC_ADDR_SIZE;
        totalSerializedSize += ACC_ADDR_SIZE;

        // Account
        size_needed = entry.second.Serialize(dst, curOffset);
        curOffset += size_needed;
        totalSerializedSize += size_needed;
    }

    return totalSerializedSize;
}

template<class DB>
int AccountStoreBase<DB>::Deserialize(const vector<unsigned char>& src,
                                      unsigned int offset)
{
    // [Total number of accounts] [Addr 1] [Account 1] [Addr 2] [Account 2] .... [Addr n] [Account n]
    // LOG_MARKER();

    try
    {
        unsigned int curOffset = offset;
        uint256_t totalNumOfAccounts
            = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;

        Address address;
        Account account;
        unsigned int numberOfAccountDeserialze = 0;
        while (numberOfAccountDeserialze < totalNumOfAccounts)
        {
            numberOfAccountDeserialze++;

            // Deserialize address
            copy(src.begin() + curOffset,
                 src.begin() + curOffset + ACC_ADDR_SIZE,
                 address.asArray().begin());
            curOffset += ACC_ADDR_SIZE;

            // Deserialize account
            // account.Deserialize(src, curOffset);
            if (account.Deserialize(src, curOffset) != 0)
            {
                LOG_GENERAL(WARNING, "We failed to init account.");
                return -1;
            }
            curOffset += ACCOUNT_SIZE;
            (*m_addressToAccount)[address] = account;
        }
        PrintAccountState();
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with AccountStoreBase::Deserialize." << ' '
                                                                << e.what());
        return -1;
    }
    return 0;
}

template<class DB>
bool AccountStoreBase<DB>::DoesAccountExist(const Address& address)
{
    LOG_MARKER();

    if (GetAccount(address) != nullptr)
    {
        return true;
    }

    return false;
}

template<class DB>
void AccountStoreBase<DB>::AddAccount(const Address& address,
                                      const Account& account)
{
    LOG_MARKER();

    if (!DoesAccountExist(address))
    {
        m_addressToAccount->insert(make_pair(address, account));
        // UpdateStateTrie(address, account);
    }
}

template<class DB>
void AccountStoreBase<DB>::AddAccount(const PubKey& pubKey,
                                      const Account& account)
{
    AddAccount(Account::GetAddressFromPublicKey(pubKey), account);
}

template<class DB>
bool AccountStoreBase<DB>::UpdateAccounts(const uint64_t& blockNum,
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
        if (amount != 0)
        {
            LOG_GENERAL(
                WARNING,
                "The balance for a contract transaction shouldn't be non-zero");
            return false;
        }
        callContract = true;
    }

    if (transaction.GetCode().size() > 0 && toAddr == NullAddress)
    {
        LOG_GENERAL(INFO, "Create Contract");

        if (amount != 0)
        {
            LOG_GENERAL(
                WARNING,
                "The balance for a contract transaction shouldn't be non-zero");
            return false;
        }

        // Create contract account
        Account* fromAccount = GetAccount(fromAddr);
        // FIXME: remove this, temporary way to test transactions
        {
            if (fromAccount == nullptr)
            {
                LOG_GENERAL(
                    WARNING,
                    "AddAccount... FIXME: remove this, temporary way to "
                    "test transactions");
                AddAccount(fromAddr, {10000000000, 0});
            }
            fromAccount = GetAccount(fromAddr);
        }

        toAddr
            = Account::GetAddressForContract(fromAddr, fromAccount->GetNonce());
        AddAccount(toAddr, {0, 0});
        Account* toAccount = GetAccount(toAddr);
        toAccount->SetCode(transaction.GetCode());
        // Store the immutable states
        toAccount->InitContract(transaction.GetData());

        m_curBlockNum = blockNum;

        if (ExportCreateContractFiles(toAccount))
        {
            SysCommand::ExecuteCmdWithOutput(GetCreateContractCmdStr());
            if (!ParseCreateContractOutput())
            {
                m_addressToAccount->erase(toAddr);
                return false;
            }
        }
    }

    TransferBalance(fromAddr, toAddr, amount);

    if (callContract)
    {
        LOG_GENERAL(INFO, "Call Contract");

        Account* toAccount = GetAccount(toAddr);
        if (toAccount == nullptr)
        {
            LOG_GENERAL(WARNING, "The target contract account doesn't exist");
            return false;
        }

        m_curBlockNum = blockNum;

        // TODO: Implement the calling of interpreter in multi-thread
        if (ExportCallContractFiles(toAccount, transaction.GetData()))
        {
            m_curContractAddr = toAddr;
            SysCommand::ExecuteCmdWithOutput(GetCallContractCmdStr());
            if (!ParseCallContractOutput())
            {
                return false;
            }
        }
    }

    IncreaseNonce(fromAddr);

    return true;
}

template<class DB>
Json::Value
AccountStoreBase<DB>::GetBlockStateJson(const uint64_t& BlockNum) const
{
    Json::Value root;
    Json::Value blockItem;
    blockItem["vname"] = "BLOCKNUMBER";
    blockItem["type"] = "BNum";
    blockItem["value"] = to_string(BlockNum);
    root.append(blockItem);
    return root;
}

template<class DB>
bool AccountStoreBase<DB>::ExportCreateContractFiles(Account* contract)
{
    LOG_MARKER();

    boost::filesystem::remove_all("./" + SCILLA_FILES);
    boost::filesystem::create_directories("./" + SCILLA_FILES);

    Json::StreamWriterBuilder writeBuilder;
    unique_ptr<Json::StreamWriter> writer(writeBuilder.newStreamWriter());
    std::ofstream os;

    // Scilla code
    os.open(INPUT_CODE);
    os << DataConversion::CharArrayToString(contract->GetCode());
    os.close();

    // Initialize Json
    os.open(INIT_JSON);
    writer->write(contract->GetInitJson(), &os);
    os.close();

    // Block Json
    os.open(INPUT_BLOCKCHAIN_JSON);
    writer->write(GetBlockStateJson(m_curBlockNum), &os);
    os.close();

    return true;
}

template<class DB>
bool AccountStoreBase<DB>::ExportCallContractFiles(
    Account* contract, const vector<unsigned char>& contractData)
{
    LOG_MARKER();

    boost::filesystem::remove_all("./" + SCILLA_FILES);
    boost::filesystem::create_directories("./" + SCILLA_FILES);

    Json::StreamWriterBuilder writeBuilder;
    unique_ptr<Json::StreamWriter> writer(writeBuilder.newStreamWriter());
    std::ofstream os;

    // Scilla code
    os.open(INPUT_CODE);
    os << DataConversion::CharArrayToString(contract->GetCode());
    os.close();

    // Initialize Json
    os.open(INIT_JSON);
    writer->write(contract->GetInitJson(), &os);
    os.close();

    // State Json
    os.open(INPUT_STATE_JSON);
    writer->write(contract->GetStorageJson(), &os);
    os.close();

    // Block Json
    os.open(INPUT_BLOCKCHAIN_JSON);
    writer->write(GetBlockStateJson(m_curBlockNum), &os);
    os.close();

    // Message Json
    Json::CharReaderBuilder readBuilder;
    unique_ptr<Json::CharReader> reader(readBuilder.newCharReader());
    Json::Value msgObj;
    string dataStr(contractData.begin(), contractData.end());
    LOG_GENERAL(INFO, "Contract Data: " << dataStr);
    string errors;
    if (reader->parse(dataStr.c_str(), dataStr.c_str() + dataStr.size(),
                      &msgObj, &errors))
    {
        os.open(INPUT_MESSAGE_JSON);
        os << dataStr;
        os.close();
    }
    else
    {
        LOG_GENERAL(WARNING,
                    "The Contract Data Json is corrupted, failed to process: "
                        << errors);
        boost::filesystem::remove_all("./" + SCILLA_FILES);
        return false;
    }
    return true;
}

template<class DB> string AccountStoreBase<DB>::GetCreateContractCmdStr()
{
    string ret = SCILLA_PATH + " -init " + INIT_JSON + " -iblockchain "
        + INPUT_BLOCKCHAIN_JSON + " -o " + OUTPUT_JSON + " -i " + INPUT_CODE;
    LOG_GENERAL(INFO, ret);
    return ret;
}

template<class DB> string AccountStoreBase<DB>::GetCallContractCmdStr()
{
    string ret = SCILLA_PATH + " -init " + INIT_JSON + " -istate "
        + INPUT_STATE_JSON + " -iblockchain " + INPUT_BLOCKCHAIN_JSON
        + " -imessage " + INPUT_MESSAGE_JSON + " -o " + OUTPUT_JSON + " -i "
        + INPUT_CODE;
    LOG_GENERAL(INFO, ret);
    return ret;
}

template<class DB> bool AccountStoreBase<DB>::ParseCreateContractOutput()
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
    Json::CharReaderBuilder builder;
    unique_ptr<Json::CharReader> reader(builder.newCharReader());
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

template<class DB>
bool AccountStoreBase<DB>::ParseCreateContractJsonOutput(
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
    else
    {
        LOG_GENERAL(WARNING,
                    "Didn't get desired json output from the interpreter for "
                    "create contract");
        return false;
    }
}

template<class DB> bool AccountStoreBase<DB>::ParseCallContractOutput()
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
    Json::CharReaderBuilder builder;
    unique_ptr<Json::CharReader> reader(builder.newCharReader());
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

template<class DB>
bool AccountStoreBase<DB>::ParseCallContractJsonOutput(const Json::Value& _json)
{
    LOG_MARKER();

    if (!_json.isMember("message") || !_json.isMember("states"))
    {
        LOG_GENERAL(WARNING, "The json output of this contract is corrupted");
        return false;
    }

    if (!_json["message"].isMember("_tag")
        || !_json["message"].isMember("_amount")
        || !_json["message"].isMember("params"))
    {
        LOG_GENERAL(
            WARNING,
            "The message in the json output of this contract is corrupted");
        return false;
    }

    int deducted = 0;

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
            Json::StreamWriterBuilder writeBuilder;
            unique_ptr<Json::StreamWriter> writer(
                writeBuilder.newStreamWriter());
            ostringstream oss;
            writer->write(s["value"], &oss);
            value = oss.str();
        }
        else
        {
            value = s["value"].asString();
        }

        Account* contractAccount = GetAccount(m_curContractAddr);
        if (vname == "_balance")
        {
            int newBalance = atoi(value.c_str());
            deducted
                = static_cast<int>(contractAccount->GetBalance()) - newBalance;
            contractAccount->SetBalance(newBalance);
        }
        else
        {
            contractAccount->SetStorage(vname, type, value);
        }
    }

    /// The process after getting the output:
    if (_json["message"]["_tag"].asString() == "Main")
    {
        Address toAddr;
        for (auto p : _json["message"]["params"])
        {
            if (p["vname"].asString() == "to"
                && p["type"].asString() == "Address")
            {
                toAddr = Address(p["value"].asString());
                break;
            }
        }
        // A hacky way of refunding the contract the number of amount for the transaction, because the balance was affected by the parsing of _balance and the 'Main' message. Need to fix in the future
        int amount = atoi(_json["message"]["_amount"].asString().c_str());
        if (amount == 0)
        {
            return true;
        }

        if (!TransferBalance(m_curContractAddr, toAddr, amount))
        {
            return false;
        }
        // what if the positive value is come from a failed function call
        if (deducted > 0)
        {
            if (!IncreaseBalance(m_curContractAddr, deducted))
            {
                return false;
            }
        }
        IncreaseNonce(m_curContractAddr);

        return true;
    }
    else
    {
        Address toAddr;
        Json::Value params;
        for (auto p : _json["message"]["params"])
        {
            if (p["vname"].asString() == "to"
                && p["type"].asString() == "Address")
            {
                toAddr = Address(p["value"].asString());
            }
            else
            {
                params.append(p);
            }
        }

        Account* toAccount = GetAccount(toAddr);
        if (toAccount == nullptr)
        {
            LOG_GENERAL(WARNING, "The target contract account doesn't exist");
            return false;
        }

        // TODO: Implement the calling of interpreter in multi-thread
        if (ExportCallContractFiles(
                toAccount,
                CompositeContractData(_json["message"]["_tag"].asString(),
                                      _json["message"]["_amount"].asString(),
                                      params)))
        {
            Address t_address = m_curContractAddr;
            m_curContractAddr = toAddr;
            SysCommand::ExecuteCmdWithOutput(GetCallContractCmdStr());
            if (ParseCallContractOutput())
            {
                IncreaseNonce(t_address);
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
}

template<class DB>
const vector<unsigned char> AccountStoreBase<DB>::CompositeContractData(
    const string& funcName, const string& amount, const Json::Value& params)
{
    LOG_MARKER();
    Json::Value obj;
    obj["_tag"] = funcName;
    obj["_amount"] = amount;
    obj["params"] = params;

    Json::StreamWriterBuilder writeBuilder;
    unique_ptr<Json::StreamWriter> writer(writeBuilder.newStreamWriter());
    ostringstream oss;
    writer->write(obj, &oss);
    string dataStr = oss.str();

    return DataConversion::StringToCharArray(dataStr);
}

template<class DB> uint256_t AccountStoreBase<DB>::GetNumOfAccounts() const
{
    LOG_MARKER();
    return m_addressToAccount->size();
}

template<class DB>
bool AccountStoreBase<DB>::IncreaseBalance(const Address& address,
                                           const uint256_t& delta)
{
    // LOG_MARKER();

    if (delta == 0)
    {
        return true;
    }

    Account* account = GetAccount(address);

    if (account != nullptr && account->IncreaseBalance(delta))
    {
        // UpdateStateTrie(address, *account);
        return true;
    }
    else if (account == nullptr)
    {
        AddAccount(address, {delta, 0});
        return true;
    }

    return false;
}

template<class DB>
bool AccountStoreBase<DB>::DecreaseBalance(const Address& address,
                                           const uint256_t& delta)
{
    // LOG_MARKER();

    if (delta == 0)
    {
        return true;
    }

    Account* account = GetAccount(address);

    if (account != nullptr && account->DecreaseBalance(delta))
    {
        // UpdateStateTrie(address, *account);
        return true;
    }
    // FIXME: remove this, temporary way to test transactions
    else if (account == nullptr)
    {
        LOG_GENERAL(WARNING,
                    "AddAccount... FIXME: remove this, temporary way to test "
                    "transactions");
        AddAccount(address, {10000000000, 0});
    }

    return false;
}

template<class DB>
bool AccountStoreBase<DB>::TransferBalance(const Address& from,
                                           const Address& to,
                                           const uint256_t& delta)
{
    // LOG_MARKER();

    if (DecreaseBalance(from, delta) && IncreaseBalance(to, delta))
    {
        return true;
    }

    return false;
}

template<class DB>
uint256_t AccountStoreBase<DB>::GetBalance(const Address& address)
{
    LOG_MARKER();

    const Account* account = GetAccount(address);

    if (account != nullptr)
    {
        return account->GetBalance();
    }

    return 0;
}

template<class DB>
bool AccountStoreBase<DB>::IncreaseNonce(const Address& address)
{
    //LOG_MARKER();

    Account* account = GetAccount(address);

    if (account != nullptr && account->IncreaseNonce())
    {
        // UpdateStateTrie(address, *account);
        return true;
    }

    return false;
}

template<class DB>
uint256_t AccountStoreBase<DB>::GetNonce(const Address& address)
{
    //LOG_MARKER();

    Account* account = GetAccount(address);

    if (account != nullptr)
    {
        return account->GetNonce();
    }

    return 0;
}

template<class DB> void AccountStoreBase<DB>::PrintAccountState()
{
    LOG_GENERAL(INFO, "Printing Account State");
    for (auto entry : *m_addressToAccount)
    {
        LOG_GENERAL(INFO, entry.first << " " << entry.second);
    }
    LOG_GENERAL(INFO, "State Root: " << GetStateRootHash());
}

template<class DB> h256 AccountStoreBase<DB>::GetStateRootHash() const
{
    LOG_MARKER();

    return m_state.root();
}

template<class DB>
bool AccountStoreBase<DB>::UpdateStateTrie(const Address& address,
                                           const Account& account)
{
    //LOG_MARKER();

    dev::RLPStream rlpStream(4);
    rlpStream << account.GetBalance() << account.GetNonce()
              << account.GetStorageRoot() << account.GetCodeHash();
    m_state.insert(address, &rlpStream.out());

    return true;
}

template<class DB> bool AccountStoreBase<DB>::UpdateStateTrieAll()
{
    bool ret = true;
    for (auto entry : *m_addressToAccount)
    {
        if (!UpdateStateTrie(entry.first, entry.second))
        {
            ret = false;
            break;
        }
    }
    return ret;
}

template<class DB> void AccountStoreBase<DB>::RepopulateStateTrie()
{
    LOG_MARKER();
    m_state.init();
    prevRoot = m_state.root();
    UpdateStateTrieAll();
}