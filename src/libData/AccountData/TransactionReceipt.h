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

#ifndef __TRANSACTIONRECEIPT_H__
#define __TRANSACTIONRECEIPT_H__

#include <boost/multiprecision/cpp_int.hpp>
#include <json/json.h>
#include <vector>

#include "LogEntry.h"
#include "Transaction.h"
#include "depends/common/FixedHash.h"
#include "libUtils/DataConversion.h"

class TransactionReceipt : public Serializable
{
    Json::Value m_tranReceiptObj;
    std::string m_tranReceiptStr;
    bool m_updated = true;
    unsigned int m_serialized_size;

public:
    TransactionReceipt() = default;
    unsigned int Serialize(std::vector<unsigned char>& dst,
                           unsigned int offset) const;
    int Deserialize(const std::vector<unsigned char>& src, unsigned int offset);
    unsigned GetSerializedSize() { return m_serialized_size; }
    void SetResult(const bool& result);
    void SetCumGas(const boost::multiprecision::uint256_t& cumGas);
    void AddEntry(const LogEntry& entry);
    std::string GetString();
    void clear();
};

class TransactionWithReceipt : public Serializable
{
    Transaction m_transaction;
    TransactionReceipt m_tranReceipt;

public:
    TransactionWithReceipt(const Transaction& tran,
                           const TransactionReceipt& tranReceipt)
        : m_transaction(tran)
        , m_tranReceipt(tranReceipt)
    {
    }

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(std::vector<unsigned char>& dst,
                           unsigned int offset) const
    {
        unsigned int old_offset = offset;

        offset += m_transaction.Serialize(dst, offset);
        offset += m_tranReceipt.Serialize(dst, offset);

        return offset - old_offset;
    }

    /// Implements the Deserialize function inherited from Serializable.
    int Deserialize(const std::vector<unsigned char>& src, unsigned int offset)
    {
        if (m_transaction.Deserialize(src, offset) == -1)
        {
            return -1;
        }
        offset += m_transaction.GetSerializedSize();

        if (m_tranReceipt.Deserialize(src, offset) == -1)
        {
            return -1;
        }
        offset += m_tranReceipt.GetSerializedSize();
        return 0;
    }

    /// Returns the size in bytes when serializing the transaction.
    unsigned int GetSerializedSize()
    {
        return m_transaction.GetSerializedSize()
            + m_tranReceipt.GetSerializedSize();
    }

    const Transaction& GetTransaction() const { return m_transaction; }
    const TransactionReceipt GetTransactionReceipt() const
    {
        return m_tranReceipt;
    }
};

#endif // __TRANSACTIONRECEIPT_H__