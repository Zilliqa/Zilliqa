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

#ifndef __ACCOUNTSTORESC_H__
#define __ACCOUNTSTORESC_H__

#include <json/json.h>

#include "AccountStoreBase.h"

class AccountStoreAtomic;

template<class MAP> class AccountStoreSC : public AccountStoreBase<MAP>
{
protected:
    uint64_t m_curBlockNum;
    Address m_curContractAddr;

    AccountStoreSC();

    bool ParseCreateContractOutput();
    bool ParseCreateContractJsonOutput(const Json::Value& _json);
    bool ParseCallContractOutput();
    bool ParseCallContractJsonOutput(const Json::Value& _json);
    Json::Value GetBlockStateJson(const uint64_t& BlockNum) const;
    string GetCreateContractCmdStr();
    string GetCallContractCmdStr();

    // Generate input for interpreter to check the correctness of contract
    void ExportCreateContractFiles(const Account& contract);

    void ExportContractFiles(const Account& contract);
    bool ExportCallContractFiles(const Account& contract,
                                 const Transaction& transaction);
    void ExportCallContractFiles(const Account& contract,
                                 const Json::Value& contractData);
    // const vector<unsigned char>
    // CompositeContractData(const string& funcName, const string& amount,
    //                       const Json::Value& params);

    bool TransferBalanceAtomic(const Address& from, const Address& to,
                               const uint256_t& delta);
    void CommitTransferBalanceAtomic();
    void DiscardTransferBalanceAtomic();

public:
    void Init() override;

    bool UpdateAccounts(const uint64_t& blockNum,
                        const Transaction& transaction);
};

#include "AccountStoreSC.tpp"

#endif // __ACCOUNTSTORESC_H__