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

#include "AccountStoreBase.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include "libUtils/SysCommand.h"

using namespace std;
using namespace boost::multiprecision;

AccountStoreBase::AccountStoreBase() 
{
	m_addressToAccount = make_shared<std::unordered_map<Address, Account>>();
}

void AccountStoreBase::Init()
{
    m_addressToAccount.clear();
}

unsigned int AccountStoreBase::Serialize(vector<unsigned char>& dst,
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
    for (auto entry : m_addressToAccount.get())
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

int AccountStoreBase::Deserialize(const vector<unsigned char>& src,
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
            m_addressToAccount.get()[address] = account;
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

bool AccountStoreBase::DoesAccountExist(const Address& address)
{
    LOG_MARKER();

    if (GetAccount(address) != nullptr)
    {
        return true;
    }

    return false;
}

void AccountStoreBase::AddAccount(const Address& address, const Account& account)
{
    LOG_MARKER();

    if (!DoesAccountExist(address))
    {
        m_addressToAccount->insert(make_pair(address, account));
        // UpdateStateTrie(address, account);
    }
}

void AccountStoreBase::AddAccount(const PubKey& pubKey, const Account& account)
{
    AddAccount(Account::GetAddressFromPublicKey(pubKey), account);
}

bool AccountStoreBase::UpdateAccounts(const uint64_t& blockNum,
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
        Account* account = GetAccount(fromAddr);
        // FIXME: remove this, temporary way to test transactions
        if (account == nullptr)
        {
            LOG_GENERAL(WARNING,
                        "AddAccount... FIXME: remove this, temporary way to "
                        "test transactions");
            AddAccount(fromAddr, {10000000000, 0});
        }

        toAddr = Account::GetAddressForContract(
            fromAddr, m_addressToAccount.get()[fromAddr].GetNonce());
        AddAccount(toAddr, {0, 0});
        m_addressToAccount.get()[toAddr].SetCode(transaction.GetCode());
        // Store the immutable states
        m_addressToAccount.get()[toAddr].InitContract(transaction.GetData());

        Account* toAccount = GetAccount(toAddr);
        if (toAccount == nullptr)
        {
            LOG_GENERAL(WARNING,
                        "The target contract account doesn't exist, which "
                        "shouldn't happen!");
            return false;
        }

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

Json::Value AccountStoreBase::GetBlockStateJson(const uint64_t& BlockNum) const
{
    Json::Value root;
    Json::Value blockItem;
    blockItem["vname"] = "BLOCKNUMBER";
    blockItem["type"] = "BNum";
    blockItem["value"] = to_string(BlockNum);
    root.append(blockItem);
    return root;
}

bool AccountStoreBase::ExportCreateContractFiles(Account* contract)
{
    LOG_MARKER();

    boost::filesystem::remove_all("./" + SCILLA_FILES);
    boost::filesystem::create_directories("./" + SCILLA_FILES);

    Json::StreamWriterBuilder writeBuilder;
    std::unique_ptr<Json::StreamWriter> writer(writeBuilder.newStreamWriter());
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

bool AccountStoreBase::ExportCallContractFiles(
    Account* contract, const vector<unsigned char>& contractData)
{
    LOG_MARKER();

    boost::filesystem::remove_all("./" + SCILLA_FILES);
    boost::filesystem::create_directories("./" + SCILLA_FILES);

    Json::StreamWriterBuilder writeBuilder;
    std::unique_ptr<Json::StreamWriter> writer(writeBuilder.newStreamWriter());
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
    std::unique_ptr<Json::CharReader> reader(readBuilder.newCharReader());
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

string AccountStoreBase::GetCreateContractCmdStr()
{
    string ret = SCILLA_PATH + " -init " + INIT_JSON + " -iblockchain "
        + INPUT_BLOCKCHAIN_JSON + " -o " + OUTPUT_JSON + " -i " + INPUT_CODE;
    LOG_GENERAL(INFO, ret);
    return ret;
}

string AccountStoreBase::GetCallContractCmdStr()
{
    string ret = SCILLA_PATH + " -init " + INIT_JSON + " -istate "
        + INPUT_STATE_JSON + " -iblockchain " + INPUT_BLOCKCHAIN_JSON
        + " -imessage " + INPUT_MESSAGE_JSON + " -o " + OUTPUT_JSON + " -i "
        + INPUT_CODE;
    LOG_GENERAL(INFO, ret);
    return ret;
}

bool AccountStoreBase::ParseCreateContractOutput()
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

bool AccountStoreBase::ParseCallContractOutput()
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

const std::vector<unsigned char>
AccountStoreBase::CompositeContractData(const std::string& funcName,
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

    return DataConversion::StringToCharArray(dataStr);
}

Account* AccountStoreBase::GetAccount(const Address& address)
{
    //LOG_MARKER();

    auto it = m_addressToAccount->find(address);
    // LOG_GENERAL(INFO, (it != m_addressToAccount.end()));
    if (it != m_addressToAccount->end())
    {
        return &it->second;
    }
}

uint256_t AccountStoreBase::GetNumOfAccounts() const
{
    LOG_MARKER();
    return m_addressToAccount->size();
}

bool AccountStoreBase::IncreaseBalance(
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

bool AccountStoreBase::DecreaseBalance(
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

bool AccountStoreBase::TransferBalance(
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
AccountStoreBase::GetBalance(const Address& address)
{
    LOG_MARKER();

    const Account* account = GetAccount(address);

    if (account != nullptr)
    {
        return account->GetBalance();
    }

    return 0;
}

bool AccountStoreBase::IncreaseNonce(const Address& address)
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

boost::multiprecision::uint256_t AccountStoreBase::GetNonce(const Address& address)
{
    //LOG_MARKER();

    Account* account = GetAccount(address);

    if (account != nullptr)
    {
        return account->GetNonce();
    }

    return 0;
}

void AccountStoreBase::PrintAccountState()
{
    LOG_MARKER();

    LOG_GENERAL(INFO, "Printing Account State");
    for (auto entry : m_addressToAccount.get())
    {
        LOG_GENERAL(INFO, entry.first << " " << entry.second);
    }
    LOG_GENERAL(INFO, "State Root: " << GetStateRootHash());
}