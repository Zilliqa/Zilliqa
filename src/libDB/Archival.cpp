/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#include "Archival.h"
#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>
#include "libMediator/Mediator.h"
#include "libUtils/DetachedFunction.h"

using namespace std;

unsigned int REFRESH_DELAY = 5;

void Archival::InitSync() {
  auto func = [this]() -> void {
    if (!m_mediator.m_node->GetOfflineLookups(true)) {
      LOG_GENERAL(WARNING, "Cannot sync currently");
      return;
    }

    uint64_t dsBlockNum = 0;
    uint64_t txBlockNum = 0;

    // m_synchronizer.FetchInitialDSInfo(m_mediator.m_lookup);
    while (true) {
      if (m_mediator.m_dsBlockChain.GetBlockCount() != 1) {
        dsBlockNum = m_mediator.m_dsBlockChain.GetBlockCount();
      }
      if (m_mediator.m_txBlockChain.GetBlockCount() != 1) {
        txBlockNum = m_mediator.m_txBlockChain.GetBlockCount();
      }
      LOG_GENERAL(INFO,
                  "TxBlockNum " << txBlockNum << " DSBlockNum: " << dsBlockNum);
      m_mediator.m_lookup->ComposeAndSendGetDirectoryBlocksFromSeed(
          m_mediator.m_blocklinkchain.GetLatestIndex() + 1);
      m_synchronizer.FetchLatestTxBlocks(m_mediator.m_lookup, txBlockNum);
      m_synchronizer.FetchDSInfo(m_mediator.m_lookup);
      m_synchronizer.FetchLatestState(m_mediator.m_lookup);

      if (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0) {
        if (!m_mediator.m_lookup->CheckStateRoot()) {
          LOG_GENERAL(WARNING, "Archival State Root mis-match");
        }
      }
      m_mediator.m_lookup->GetShardFromLookup();
      if (m_mediator.m_currentEpochNum > 1) {
        SendFetchMicroBlockInfo();
        SendFetchTxn();
      }
      if (m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0) {
        LOG_GENERAL(INFO,
                    "Sleep for " << REFRESH_DELAY + POW_WINDOW_IN_SECONDS);
        this_thread::sleep_for(
            chrono::seconds(REFRESH_DELAY + POW_WINDOW_IN_SECONDS));
      } else {
        this_thread::sleep_for(chrono::seconds(REFRESH_DELAY));
      }
    }
  };
  DetachedFunction(1, func);
}

bool Archival::Execute(
    [[gnu::unused]] const std::vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset, [[gnu::unused]] const Peer& from) {
  LOG_MARKER();
  return true;
}

void Archival::Init() {
  m_mediator.m_dsBlockChain.Reset();
  m_mediator.m_txBlockChain.Reset();
  m_mediator.m_blocklinkchain.Reset();
  {
    std::lock_guard<mutex> lock(m_mediator.m_mutexDSCommittee);
    m_mediator.m_DSCommittee->clear();
  }
  AccountStore::GetInstance().Init();

  m_synchronizer.InitializeGenesisBlocks(m_mediator.m_dsBlockChain,
                                         m_mediator.m_txBlockChain);
  const auto& dsBlock = m_mediator.m_dsBlockChain.GetBlock(0);
  m_mediator.m_blocklinkchain.AddBlockLink(0, 0, BlockType::DS,
                                           dsBlock.GetBlockHash());
}

bool Archival::AddToFetchMicroBlockInfo(const uint64_t& blockNum,
                                        const uint32_t shardId) {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexMicroBlockInfo);
  LOG_GENERAL(INFO,
              "Added " << blockNum << " " << shardId << " to fetch microBlock");
  m_fetchMicroBlockInfo[blockNum].push_back(shardId);

  return true;
}

bool Archival::RemoveFromFetchMicroBlockInfo(const uint64_t& blockNum,
                                             const uint32_t shardId) {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexMicroBlockInfo);

  if (m_fetchMicroBlockInfo.count(blockNum) == 0) {
    LOG_GENERAL(INFO, "Already empty " << blockNum << " shard id" << shardId);
    return false;
  }

  vector<uint32_t>& shard_ids = m_fetchMicroBlockInfo.at(blockNum);

  auto position = find(shard_ids.begin(), shard_ids.end(), shardId);
  if (position != shard_ids.end()) {
    shard_ids.erase(position);
    return true;
  } else {
    LOG_GENERAL(WARNING,
                "Could not find " << blockNum << " shard id " << shardId);
    return false;
  }
}

void Archival::SendFetchMicroBlockInfo() {
  LOG_MARKER();
  lock_guard<mutex> g(m_mutexMicroBlockInfo);
  for (const auto& info : m_fetchMicroBlockInfo) {
    if (info.second.size() == 0) {
      m_fetchMicroBlockInfo.erase(info.first);
    } else {
      LOG_GENERAL(INFO, "Sending fetch microBlock "
                            << "..." << info.first);
      for (const auto& shard_id : info.second) {
        LOG_GENERAL(INFO, "Shard id " << shard_id);
      }
    }
  }
  m_mediator.m_lookup->SendGetMicroBlockFromLookup(m_fetchMicroBlockInfo);
}

void Archival::AddToUnFetchedTxn(const vector<TxnHash>& txnhashes) {
  lock_guard<mutex> g(m_mutexUnfetchedTxns);

  LOG_GENERAL(INFO, "Add " << txnhashes.size() << " to unfetched txns");
  copy(txnhashes.begin(), txnhashes.end(),
       inserter(m_unfetchedTxns, m_unfetchedTxns.end()));
}

void Archival::AddTxnToDB(const vector<TransactionWithReceipt>& txns,
                          BaseDB& db) {
  lock_guard<mutex> g(m_mutexUnfetchedTxns);

  LOG_GENERAL(INFO, " Got " << txns.size() << " from lookup");
  for (const auto& txn : txns) {
    const TxnHash& txhash = txn.GetTransaction().GetTranID();

    if (m_unfetchedTxns.find(txhash) != m_unfetchedTxns.end()) {
      db.InsertTxn(txn);
      m_unfetchedTxns.erase(txhash);
    } else {
      LOG_GENERAL(WARNING,
                  "Hash " << txhash << " not in my unfetched txn list");
    }
  }
}

void Archival::SendFetchTxn() {
  LOG_MARKER();
  lock_guard<mutex> g(m_mutexUnfetchedTxns);
  LOG_GENERAL(INFO, "Send for " << m_unfetchedTxns.size() << " to lookup");

  vector<TxnHash> txnVec(m_unfetchedTxns.begin(), m_unfetchedTxns.end());
  m_mediator.m_lookup->SendGetTxnFromLookup(txnVec);
}

Archival::Archival(Mediator& mediator) : m_mediator(mediator) {}

Archival::~Archival() {}