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
#include <leveldb/db.h>

#include "AccountStore.h"
#include "Address.h"
#include "depends/common/RLP.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include "libUtils/SysCommand.h"

using namespace std;
using namespace boost::multiprecision;

AccountStore::AccountStore()
    : m_db("state")
{
    m_state = SecureTrieDB<Address, dev::OverlayDB>(&m_db);
}

AccountStore::~AccountStore()
{
    // boost::filesystem::remove_all("./state");
}

void AccountStore::Init()
{
    LOG_MARKER();
    m_addressToAccount.clear();
    ContractStorage::GetContractStorage().GetStateDB().ResetDB();
    m_db.ResetDB();
    m_state.init();
    prevRoot = m_state.root();
}

unsigned int AccountStore::Serialize(vector<unsigned char>& dst,
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
    for (auto entry : m_addressToAccount)
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

int AccountStore::Deserialize(const vector<unsigned char>& src,
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
        this->Init();
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
            m_addressToAccount[address] = account;
            UpdateStateTrie(address, account);
            // MoveUpdatesToDisk();
        }
        PrintAccountState();
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with AccountStore::Deserialize." << ' ' << e.what());
        return -1;
    }
    return 0;
}

AccountStore& AccountStore::GetInstance()
{
    static AccountStore accountstore;
    return accountstore;
}

bool AccountStore::DoesAccountExist(const Address& address)
{
    LOG_MARKER();

    if (GetAccount(address) != nullptr)
    {
        return true;
    }

    return false;
}

void AccountStore::AddAccount(const Address& address, const Account& account)
{
    LOG_MARKER();

    if (!DoesAccountExist(address))
    {
        m_addressToAccount.insert(make_pair(address, account));
        // UpdateStateTrie(address, account);
    }
}

void AccountStore::AddAccount(const PubKey& pubKey, const Account& account)
{
    AddAccount(Account::GetAddressFromPublicKey(pubKey), account);
}

void AccountStore::UpdateAccounts(const uint64_t& blockNum,
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
        callContract = true;
    }

    if (transaction.GetCode().size() > 0 && toAddr == NullAddress)
    {
        LOG_GENERAL(INFO, "Create Contract");
        // Create contract account
        Account* account = GetAccount(fromAddr);
        // TODO: remove this, temporary way to test transactions
        if (account == nullptr)
        {
            AddAccount(fromAddr, {10000000000, 0});
        }

        toAddr = Account::GetAddressForContract(
            fromAddr, m_addressToAccount[fromAddr].GetNonce());
        AddAccount(toAddr, {0, 0});
        m_addressToAccount[toAddr].SetCode(transaction.GetCode());
        // Store the immutable states
        m_addressToAccount[toAddr].InitContract(transaction.GetData());
    }

    TransferBalance(fromAddr, toAddr, amount);
    IncreaseNonce(fromAddr);

    if (callContract)
    {
        LOG_GENERAL(INFO, "Call Contract");

        std::lock_guard<std::mutex> lk(m_mutexCallInterpreter);

        Account* toAccount = GetAccount(toAddr);
        if (toAccount == nullptr)
        {
            LOG_GENERAL(WARNING, "The target contract account doesn't exist");
            return;
        }

        m_curBlockNum = blockNum;

        // TODO: Implement the calling of interpreter in multi-thread
        if (ExportContractFiles(toAccount, transaction.GetData()))
        {
            m_curContractAddr = toAddr;
            SysCommand::ExecuteCmdWithOutput(GetContractCmdStr());
            ParseContractOutput();
        }
    }
}

Json::Value AccountStore::GetBlockStateJson(const uint64_t& BlockNum) const
{
    Json::Value root;
    Json::Value blockItem;
    blockItem["vname"] = "BLOCKNUMBER";
    blockItem["type"] = "BNum";
    blockItem["value"] = to_string(BlockNum);
    root.append(blockItem);
    return root;
}

bool AccountStore::ExportContractFiles(
    Account*& contract, const vector<unsigned char>& contractData)
{
    LOG_MARKER();

    boost::filesystem::remove_all("./" + SCILLA_FILES);
    boost::filesystem::create_directories("./" + SCILLA_FILES);

    Json::StreamWriterBuilder writeBuilder;
    std::unique_ptr<Json::StreamWriter> writer(writeBuilder.newStreamWriter());
    std::ofstream os;

    // Scilla code
    os.open(INPUT_CODE);
    os << string(contract->GetCode().begin(), contract->GetCode().end());
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
    std::unique_ptr<Json::CharReader> reader(readBuilder.newCharReader());
    Json::Value msgObj;
    string dataStr(contractData.begin(), contractData.end());
    LOG_GENERAL(INFO, "Contract Data: " << dataStr);
    string errors;
    if (reader->parse(dataStr.c_str(), dataStr.c_str() + dataStr.size(),
                      &msgObj, &errors))
    {
        os.open(INPUT_MESSAGE_JSON);
        os << string(contractData.begin(), contractData.end());
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

string AccountStore::GetContractCmdStr()
{
    return "./" + INTERPRETER_NAME + " -init " + INIT_JSON + " -istate "
        + INPUT_STATE_JSON + " -iblockchain " + INPUT_BLOCKCHAIN_JSON
        + " -imessage " + INPUT_MESSAGE_JSON + " -o " + OUTPUT_JSON + " -i "
        + INPUT_CODE;
}

void AccountStore::ParseContractOutput()
{
    LOG_MARKER();
    ifstream in(OUTPUT_JSON, ios::binary);

    if (!in.is_open())
    {
        LOG_GENERAL(WARNING,
                    "Error Opening output file or no output file generated");
        return;
    }
    string outStr{istreambuf_iterator<char>(in), istreambuf_iterator<char>()};
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    string errors;
    if (reader->parse(outStr.c_str(), outStr.c_str() + outStr.size(), &root,
                      &errors))
    {
        ParseJsonOutput(root);
    }
    else
    {
        LOG_GENERAL(WARNING,
                    "Failed to parse contract output json: " << errors);
    }
}

void AccountStore::ParseJsonOutput(const Json::Value& _json)
{
    LOG_MARKER();
    // the _json actually refers to the Array of Contracts,
    // one transaction can affect multiple contracts by one call

    if (!_json.isMember("message") || !_json.isMember("states"))
    {
        LOG_GENERAL(WARNING, "The json output of this contract is corrupted");
        return;
    }

    if (!_json["message"].isMember("_tag")
        || !_json["message"].isMember("_amount")
        || !_json["message"].isMember("params"))
    {
        LOG_GENERAL(
            WARNING,
            "The message in the json output of this contract is corrupted");
        return;
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
            Json::StreamWriterBuilder writeBuilder;
            std::unique_ptr<Json::StreamWriter> writer(
                writeBuilder.newStreamWriter());
            ostringstream oss;
            writer->write(s["value"], &oss);
            value = oss.str();
        }
        else
        {
            value = s["value"].asString();
        }

        if (vname == "_balance")
        {
            m_addressToAccount[m_curContractAddr].SetBalance(
                atoi(value.c_str()));
        }
        else
        {
            m_addressToAccount[m_curContractAddr].SetStorage(vname, type,
                                                             value);
        }
    }

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
        TransferBalance(m_curContractAddr, toAddr,
                        atoi(_json["message"]["_amount"].asString().c_str()));
        IncreaseNonce(m_curContractAddr);
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
            return;
        }

        // TODO: Implement the calling of interpreter in multi-thread
        if (ExportContractFiles(
                toAccount,
                CompositeContractData(_json["message"]["_tag"].asString(),
                                      _json["message"]["_amount"].asString(),
                                      params)))
        {
            m_curContractAddr = toAddr;
            SysCommand::ExecuteCmdWithOutput(GetContractCmdStr());
            ParseContractOutput();
        }
    }
}

const std::vector<unsigned char>
AccountStore::CompositeContractData(const std::string& funcName,
                                    const std::string& amount,
                                    const Json::Value& params)
{
    LOG_MARKER();
    Json::Value obj;
    obj["_tag"] = funcName;
    obj["_amount"] = amount;
    obj["params"] = params;

    Json::StreamWriterBuilder writeBuilder;
    std::unique_ptr<Json::StreamWriter> writer(writeBuilder.newStreamWriter());
    ostringstream oss;
    writer->write(obj, &oss);
    string dataStr = oss.str();

    std::vector<unsigned char> vect;
    vect.insert(vect.begin(), dataStr.begin(), dataStr.end());
    return vect;
}

Account* AccountStore::GetAccount(const Address& address)
{
    //LOG_MARKER();

    auto it = m_addressToAccount.find(address);
    // LOG_GENERAL(INFO, (it != m_addressToAccount.end()));
    if (it != m_addressToAccount.end())
    {
        return &it->second;
    }

    string accountDataString = m_state.at(address);
    if (accountDataString.empty())
    {
        return nullptr;
    }

    dev::RLP accountDataRLP(accountDataString);
    if (accountDataRLP.itemCount() != 4)
    {
        LOG_GENERAL(WARNING, "Account data corrupted");
        return nullptr;
    }

    auto it2 = m_addressToAccount.emplace(
        std::piecewise_construct, std::forward_as_tuple(address),
        std::forward_as_tuple(
            accountDataRLP[0].toInt<boost::multiprecision::uint256_t>(),
            accountDataRLP[1].toInt<boost::multiprecision::uint256_t>()));

    // Code Hash
    if (accountDataRLP[3].toHash<dev::h256>() != dev::h256())
    {
        // Extract Code Content
        it2.first->second.SetCode(
            ContractStorage::GetContractStorage().GetContractCode(address));
        if (accountDataRLP[3].toHash<dev::h256>()
            != it2.first->second.GetCodeHash())
        {
            LOG_GENERAL(WARNING, "Account Code Content doesn't match Code Hash")
            m_addressToAccount.erase(it2.first);
            return nullptr;
        }
        // Storage Root
        it2.first->second.SetStorageRoot(accountDataRLP[2].toHash<dev::h256>());
    }

    return &it2.first->second;
}

uint256_t AccountStore::GetNumOfAccounts() const
{
    LOG_MARKER();
    return m_addressToAccount.size();
}

bool AccountStore::UpdateStateTrieAll()
{
    bool ret = true;
    for (auto entry : m_addressToAccount)
    {
        if (!UpdateStateTrie(entry.first, entry.second))
        {
            ret = false;
            break;
        }
    }
    return ret;
}

bool AccountStore::UpdateStateTrie(const Address& address,
                                   const Account& account)
{
    //LOG_MARKER();

    dev::RLPStream rlpStream(4);
    rlpStream << account.GetBalance() << account.GetNonce()
              << account.GetStorageRoot() << account.GetCodeHash();
    m_state.insert(address, &rlpStream.out());

    return true;
}

bool AccountStore::IncreaseBalance(
    const Address& address, const boost::multiprecision::uint256_t& delta)
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

bool AccountStore::DecreaseBalance(
    const Address& address, const boost::multiprecision::uint256_t& delta)
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
    // TODO: remove this, temporary way to test transactions
    else if (account == nullptr)
    {
        AddAccount(address, {10000000000, 0});
    }

    return false;
}

bool AccountStore::TransferBalance(
    const Address& from, const Address& to,
    const boost::multiprecision::uint256_t& delta)
{
    // LOG_MARKER();

    if (DecreaseBalance(from, delta) && IncreaseBalance(to, delta))
    {
        return true;
    }

    return false;
}

boost::multiprecision::uint256_t
AccountStore::GetBalance(const Address& address)
{
    LOG_MARKER();

    const Account* account = GetAccount(address);

    if (account != nullptr)
    {
        return account->GetBalance();
    }

    return 0;
}

bool AccountStore::IncreaseNonce(const Address& address)
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

boost::multiprecision::uint256_t AccountStore::GetNonce(const Address& address)
{
    //LOG_MARKER();

    Account* account = GetAccount(address);

    if (account != nullptr)
    {
        return account->GetNonce();
    }

    return 0;
}

dev::h256 AccountStore::GetStateRootHash() const
{
    LOG_MARKER();

    return m_state.root();
}

void AccountStore::MoveRootToDisk(const dev::h256& root)
{
    //convert h256 to bytes
    if (!BlockStorage::GetBlockStorage().PutMetadata(STATEROOT, root.asBytes()))
        LOG_GENERAL(INFO, "FAIL: Put metadata failed");
}

void AccountStore::MoveUpdatesToDisk()
{
    LOG_MARKER();

    ContractStorage::GetContractStorage().GetStateDB().commit();
    for (auto i : m_addressToAccount)
    {
        if (ContractStorage::GetContractStorage().PutContractCode(
                i.first, i.second.GetCode()))
        {
            LOG_GENERAL(WARNING, "Write Contract Code to Disk Failed");
            continue;
        }
        i.second.Commit();
    }
    m_state.db()->commit();
    prevRoot = m_state.root();
    MoveRootToDisk(prevRoot);
}

void AccountStore::DiscardUnsavedUpdates()
{
    LOG_MARKER();

    ContractStorage::GetContractStorage().GetStateDB().rollback();
    for (auto i : m_addressToAccount)
    {
        i.second.RollBack();
    }
    m_state.db()->rollback();
    m_state.setRoot(prevRoot);
    m_addressToAccount.clear();
}

void AccountStore::PrintAccountState()
{
    LOG_MARKER();

    LOG_GENERAL(INFO, "Printing Account State");
    for (auto entry : m_addressToAccount)
    {
        LOG_GENERAL(INFO, entry.first << " " << entry.second);
    }
    LOG_GENERAL(INFO, "State Root: " << GetStateRootHash());
}

bool AccountStore::RetrieveFromDisk()
{
    LOG_MARKER();
    std::vector<unsigned char> rootBytes;
    if (!BlockStorage::GetBlockStorage().GetMetadata(STATEROOT, rootBytes))
    {
        return false;
    }
    dev::h256 root(rootBytes);
    m_state.setRoot(root);
    for (auto i : m_state)
    {
        Address address(i.first);
        LOG_GENERAL(INFO, "Address: " << address.hex());
        dev::RLP rlp(i.second);
        if (rlp.itemCount() != 4)
        {
            LOG_GENERAL(WARNING, "Account data corrupted");
            continue;
        }
        Account account(rlp[0].toInt<boost::multiprecision::uint256_t>(),
                        rlp[1].toInt<boost::multiprecision::uint256_t>());
        // Code Hash
        if (rlp[3].toHash<dev::h256>() != dev::h256())
        {
            // Extract Code Content
            account.SetCode(
                ContractStorage::GetContractStorage().GetContractCode(address));
            if (rlp[3].toHash<dev::h256>() != account.GetCodeHash())
            {
                LOG_GENERAL(WARNING,
                            "Account Code Content doesn't match Code Hash")
                continue;
            }
            // Storage Root
            account.SetStorageRoot(rlp[2].toHash<dev::h256>());
        }
        m_addressToAccount.insert({address, account});
    }
    return true;
}

void AccountStore::RepopulateStateTrie()
{
    LOG_MARKER();
    m_state.init();
    prevRoot = m_state.root();
    UpdateStateTrieAll();
}