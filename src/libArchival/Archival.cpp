/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
                    "Sleep for " << REFRESH_DELAY + POW_WINDOW_IN_SECONDS +
                                        POWPACKETSUBMISSION_WINDOW_IN_SECONDS);
        this_thread::sleep_for(
            chrono::seconds(REFRESH_DELAY + POW_WINDOW_IN_SECONDS +
                            POWPACKETSUBMISSION_WINDOW_IN_SECONDS));
      } else {
        this_thread::sleep_for(chrono::seconds(REFRESH_DELAY));
      }
    }
  };
  DetachedFunction(1, func);
}

bool Archival::Execute([[gnu::unused]] const bytes& message,
                       [[gnu::unused]] unsigned int offset,
                       [[gnu::unused]] const Peer& from) {
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

bool Archival::AddToFetchMicroBlockInfo(const BlockHash& microBlockHash) {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexMicroBlockInfo);
  LOG_GENERAL(INFO, "Added " << microBlockHash << " to fetch microBlock");
  m_fetchMicroBlockInfo.emplace_back(microBlockHash);

  return true;
}

bool Archival::RemoveFromFetchMicroBlockInfo(const BlockHash& microBlockHash) {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexMicroBlockInfo);

  auto position = find(m_fetchMicroBlockInfo.begin(),
                       m_fetchMicroBlockInfo.end(), microBlockHash);
  if (position != m_fetchMicroBlockInfo.end()) {
    m_fetchMicroBlockInfo.erase(position);
    return true;
  } else {
    LOG_GENERAL(WARNING, "Could not find hash " << microBlockHash);
    return false;
  }
}

void Archival::SendFetchMicroBlockInfo() {
  LOG_MARKER();
  lock_guard<mutex> g(m_mutexMicroBlockInfo);
  for (const auto& mbHash : m_fetchMicroBlockInfo) {
    LOG_GENERAL(INFO, "Sending fetch microBlock Hash " << mbHash.hex());
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
