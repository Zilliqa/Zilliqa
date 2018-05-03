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

#include "TxnRootComputation.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"

TxnHash ComputeTransactionsRoot(const std::vector<TxnHash>& transactionHashes)
{
    LOG_MARKER();

    dev::MemoryDB tm;
    dev::GenericTrieDB<dev::MemoryDB> transactionsTrie(&tm);
    transactionsTrie.init();

    int txnCount = 0;
    for (auto it = transactionHashes.begin(); it != transactionHashes.end();
         it++)
    {
        std::vector<unsigned char> serializedTxn;
        serializedTxn.resize(TRAN_HASH_SIZE);
        copy(it->asArray().begin(), it->asArray().end(), serializedTxn.begin());
        dev::RLPStream k;
        k << txnCount;
        txnCount++;
        transactionsTrie.insert(&k.out(), serializedTxn);
    }

    TxnHash txnRoot;
    std::vector<unsigned char> t = transactionsTrie.root().asBytes();
    copy(t.begin(), t.end(), txnRoot.asArray().begin());

    return txnRoot;
}

TxnHash
ComputeTransactionsRoot(const std::list<Transaction>& receivedTransactions,
                        const std::list<Transaction>& submittedTransactions)
{
    dev::MemoryDB tm;
    dev::GenericTrieDB<dev::MemoryDB> transactionsTrie(&tm);
    transactionsTrie.init();

    int txnCount = 0;
    for (auto it = receivedTransactions.begin();
         it != receivedTransactions.end(); it++)
    {
        std::vector<unsigned char> serializedTxn;
        serializedTxn.resize(TRAN_HASH_SIZE);
        copy(it->GetTranID().begin(), it->GetTranID().end(),
             serializedTxn.begin());

        dev::RLPStream k;
        k << txnCount;
        txnCount++;

        transactionsTrie.insert(&k.out(), serializedTxn);
        // LOG_GENERAL(INFO, "Inserted to trie" << txnCount);
    }
    for (auto it = submittedTransactions.begin();
         it != submittedTransactions.end(); it++)
    {
        std::vector<unsigned char> serializedTxn;
        serializedTxn.resize(TRAN_HASH_SIZE);
        copy(it->GetTranID().begin(), it->GetTranID().end(),
             serializedTxn.begin());

        dev::RLPStream k;
        k << txnCount;
        txnCount++;

        transactionsTrie.insert(&k.out(), serializedTxn);
        // LOG_GENERAL(INFO, "Inserted to trie" << txnCount);
    }

    TxnHash txnRoot;
    std::vector<unsigned char> t = transactionsTrie.root().asBytes();
    copy(t.begin(), t.end(), txnRoot.asArray().begin());

    return txnRoot;
}

TxnHash ComputeTransactionsRoot(
    const std::unordered_map<TxnHash, Transaction>& receivedTransactions,
    const std::unordered_map<TxnHash, Transaction>& submittedTransactions)
{
    LOG_MARKER();

    dev::MemoryDB tm;
    dev::GenericTrieDB<dev::MemoryDB> transactionsTrie(&tm);
    transactionsTrie.init();

    int txnCount = 0;
    for (auto& it : receivedTransactions)
    {
        std::vector<unsigned char> serializedTxn;
        serializedTxn.resize(TRAN_HASH_SIZE);
        copy(it.first.begin(), it.first.end(), serializedTxn.begin());

        dev::RLPStream k;
        k << txnCount;
        txnCount++;

        transactionsTrie.insert(&k.out(), serializedTxn);
        // LOG_GENERAL(INFO, "Inserted to trie" << txnCount);
    }
    for (auto& it : submittedTransactions)
    {
        std::vector<unsigned char> serializedTxn;
        serializedTxn.resize(TRAN_HASH_SIZE);
        copy(it.first.begin(), it.first.end(), serializedTxn.begin());

        dev::RLPStream k;
        k << txnCount;
        txnCount++;

        transactionsTrie.insert(&k.out(), serializedTxn);
        // LOG_GENERAL(INFO, "Inserted to trie" << txnCount);
    }

    TxnHash txnRoot;
    std::vector<unsigned char> t = transactionsTrie.root().asBytes();
    copy(t.begin(), t.end(), txnRoot.asArray().begin());

    return txnRoot;
}