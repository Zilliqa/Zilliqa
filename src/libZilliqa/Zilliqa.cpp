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

#include <chrono>

#include "Zilliqa.h"
#include "common/Constants.h"
#include "common/MessageNames.h"
#include "common/Serializable.h"
#include "depends/safeserver/safehttpserver.h"
#include "depends/safeserver/safetcpsocketserver.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Address.h"
#include "libNetwork/Guard.h"
#include "libServer/GetWorkServer.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/UpgradeManager.h"

using namespace std;
using namespace jsonrpc;

void Zilliqa::LogSelfNodeInfo(const PairOfKey& key, const Peer& peer) {
  bytes tmp1;
  bytes tmp2;

  key.first.Serialize(tmp1, 0);
  key.second.Serialize(tmp2, 0);

  LOG_PAYLOAD(INFO, "Public Key", tmp2, PUB_KEY_SIZE * 2);

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Reset();
  bytes message;
  key.second.Serialize(message, 0);
  sha2.Update(message, 0, PUB_KEY_SIZE);
  const bytes& tmp3 = sha2.Finalize();
  Address toAddr;
  copy(tmp3.end() - ACC_ADDR_SIZE, tmp3.end(), toAddr.asArray().begin());

  LOG_GENERAL(INFO, "My address is " << toAddr << " and port is "
                                     << peer.m_listenPortHost);
}

/*static*/ std::string Zilliqa::FormatMessageName(unsigned char msgType,
                                                  unsigned char instruction) {
  const std::string InvalidMessageType = "INVALID_MESSAGE";
  if (msgType >= ARRAY_SIZE(MessageTypeStrings)) {
    return InvalidMessageType;
  }

  if (NULL == MessageTypeInstructionStrings[msgType]) {
    return InvalidMessageType;
  }

  if (instruction >= MessageTypeInstructionSize[msgType]) {
    return InvalidMessageType;
  }

  return MessageTypeStrings[msgType] + "_" +
         MessageTypeInstructionStrings[msgType][instruction];
}

void Zilliqa::ProcessMessage(pair<bytes, Peer>* message) {
  if (message->first.size() >= MessageOffset::BODY) {
    const unsigned char msg_type = message->first.at(MessageOffset::TYPE);

    // To-do: Remove consensus user and peer manager placeholders
    Executable* msg_handlers[] = {NULL, &m_ds, &m_n, NULL, &m_lookup};

    const unsigned int msg_handlers_count =
        sizeof(msg_handlers) / sizeof(Executable*);

    if (msg_type < msg_handlers_count) {
      if (msg_handlers[msg_type] == NULL) {
        LOG_GENERAL(WARNING, "Message type NULL");
        delete message;
        return;
      }

      std::chrono::time_point<std::chrono::high_resolution_clock> tpStart;
      std::string msgName;
      if (ENABLE_CHECK_PERFORMANCE_LOG) {
        const auto ins_byte = message->first.at(MessageOffset::INST);
        msgName = FormatMessageName(msg_type, ins_byte);
        LOG_GENERAL(INFO, MessageSizeKeyword << msgName << " "
                                             << message->first.size());

        tpStart = std::chrono::high_resolution_clock::now();
      }

      bool result = msg_handlers[msg_type]->Execute(
          message->first, MessageOffset::INST, message->second);

      if (ENABLE_CHECK_PERFORMANCE_LOG) {
        auto tpNow = std::chrono::high_resolution_clock::now();
        auto timeInMicro = static_cast<int64_t>(
            (std::chrono::duration<double, std::micro>(tpNow - tpStart))
                .count());
        LOG_GENERAL(
            INFO, MessgeTimeKeyword << msgName << " " << timeInMicro << " us");
      }

      if (!result) {
        // To-do: Error recovery
      }
    } else {
      LOG_GENERAL(WARNING, "Unknown message type " << std::hex
                                                   << (unsigned int)msg_type);
    }
  }

  delete message;
}

Zilliqa::Zilliqa(const PairOfKey& key, const Peer& peer, SyncType syncType,
                 bool toRetrieveHistory)
    : m_mediator(key, peer),
      m_ds(m_mediator),
      m_lookup(m_mediator, syncType),
      m_n(m_mediator, syncType, toRetrieveHistory),
      m_msgQueue(MSGQUEUE_SIZE)

{
  LOG_MARKER();

  // Launch the thread that reads messages from the queue
  auto funcCheckMsgQueue = [this]() mutable -> void {
    pair<bytes, Peer>* message = NULL;
    while (true) {
      while (m_msgQueue.pop(message)) {
        // For now, we use a thread pool to handle this message
        // Eventually processing will be single-threaded
        m_queuePool.AddJob(
            [this, message]() mutable -> void { ProcessMessage(message); });
      }
      std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
  };
  DetachedFunction(1, funcCheckMsgQueue);

  m_validator = make_shared<Validator>(m_mediator);

  m_mediator.RegisterColleagues(&m_ds, &m_n, &m_lookup, m_validator.get());

  {
    lock_guard<mutex> lock(m_mediator.m_mutexInitialDSCommittee);
    if (!UpgradeManager::GetInstance().LoadInitialDS(
            *m_mediator.m_initialDSCommittee)) {
      LOG_GENERAL(WARNING, "Unable to load initial DS comm");
    }
  }

  if (ARCHIVAL_LOOKUP && !LOOKUP_NODE_MODE) {
    LOG_GENERAL(FATAL, "Archvial lookup is true but not lookup ");
  }

  P2PComm::GetInstance().SetSelfPeer(peer);
  P2PComm::GetInstance().SetSelfKey(key);

  // Clear any existing diagnostic data from previous runs
  BlockStorage::GetBlockStorage().ResetDB(BlockStorage::DIAGNOSTIC_NODES);
  BlockStorage::GetBlockStorage().ResetDB(BlockStorage::DIAGNOSTIC_COINBASE);

  if (GUARD_MODE) {
    // Setting the guard upon process launch
    Guard::GetInstance().Init();

    if (Guard::GetInstance().IsNodeInDSGuardList(m_mediator.m_selfKey.second)) {
      LOG_GENERAL(INFO, "Current node is a DS guard");
    } else if (Guard::GetInstance().IsNodeInShardGuardList(
                   m_mediator.m_selfKey.second)) {
      LOG_GENERAL(INFO, "Current node is a shard guard");
    }
  }

  auto func = [this, toRetrieveHistory, syncType, key, peer]() mutable -> void {
    LogSelfNodeInfo(key, peer);
    while (!m_n.Install((SyncType)syncType, toRetrieveHistory)) {
      if (LOOKUP_NODE_MODE && !ARCHIVAL_LOOKUP) {
        syncType = SyncType::LOOKUP_SYNC;
        m_mediator.m_lookup->SetSyncType(SyncType::LOOKUP_SYNC);
        break;
      } else if (toRetrieveHistory && (SyncType::NEW_LOOKUP_SYNC == syncType ||
                                       SyncType::NEW_SYNC == syncType)) {
        if (SyncType::NEW_LOOKUP_SYNC == syncType) {
          m_lookup.CleanVariables();
        } else {
          m_n.CleanVariables();
        }
        while (!m_n.DownloadPersistenceFromS3()) {
          LOG_GENERAL(
              WARNING,
              "Downloading persistence from S3 has failed. Will try again!");
          this_thread::sleep_for(chrono::seconds(RETRY_REJOINING_TIMEOUT));
        }
        if (!BlockStorage::GetBlockStorage().RefreshAll()) {
          LOG_GENERAL(WARNING, "BlockStorage::RefreshAll failed");
        }
        if (!AccountStore::GetInstance().RefreshDB()) {
          LOG_GENERAL(WARNING, "AccountStore::RefreshDB failed");
        }
      } else {
        m_mediator.m_lookup->SetSyncType(SyncType::NO_SYNC);
        bool isDsNode = false;
        for (const auto& ds : *m_mediator.m_DSCommittee) {
          if (ds.first == m_mediator.m_selfKey.second) {
            isDsNode = true;
            m_ds.RejoinAsDS(false);
            break;
          }
        }
        if (!isDsNode) {
          m_n.RejoinAsNormal();
        }
        break;
      }
    }

    switch (syncType) {
      case SyncType::NO_SYNC:
        LOG_GENERAL(INFO, "No Sync Needed");
        break;
      case SyncType::NEW_SYNC:
        LOG_GENERAL(INFO, "Sync as a new node");
        if (toRetrieveHistory) {
          m_n.m_runFromLate = true;
          m_n.StartSynchronization();
        } else {
          LOG_GENERAL(WARNING,
                      "Error: Sync for new node should retrieve history as "
                      "much as possible!");
        }
        break;
      case SyncType::NEW_LOOKUP_SYNC:
        LOG_GENERAL(INFO, "Sync as a new lookup node");
        if (toRetrieveHistory) {
          m_lookup.InitSync();
        } else {
          LOG_GENERAL(FATAL,
                      "Error: Sync for new lookup should retrieve history as "
                      "much as possible");
        }
        break;
      case SyncType::NORMAL_SYNC:
        LOG_GENERAL(INFO, "Sync as a normal node");
        m_n.m_runFromLate = true;
        m_n.StartSynchronization();
        break;
      case SyncType::DS_SYNC:
        LOG_GENERAL(INFO, "Sync as a ds node");
        m_ds.StartSynchronization();
        break;
      case SyncType::LOOKUP_SYNC:
        LOG_GENERAL(INFO, "Sync as a lookup node");
        m_lookup.StartSynchronization();
        break;
      case SyncType::RECOVERY_ALL_SYNC:
        LOG_GENERAL(INFO, "Recovery all nodes, no Sync Needed");
        // When doing recovery, make sure to let other lookups know I'm back
        // online
        if (LOOKUP_NODE_MODE) {
          if (!m_mediator.m_lookup->GetMyLookupOnline(true)) {
            LOG_GENERAL(WARNING, "Failed to notify lookups I am back online");
          }
        }
        break;
      case SyncType::GUARD_DS_SYNC:
        LOG_GENERAL(INFO, "Sync as a ds guard node");
        m_ds.m_awaitingToSubmitNetworkInfoUpdate = true;
        // downloads and sync from the persistence of incremental db and
        // and rejoins the network as ds guard member
        m_mediator.m_lookup->SetSyncType(SyncType::NO_SYNC);
        m_ds.RejoinAsDS(false);
        break;
      case SyncType::DB_VERIF:
        LOG_GENERAL(INFO, "Intitialize DB verification");
        m_n.ValidateDB();
        std::this_thread::sleep_for(std::chrono::seconds(10));
        raise(SIGKILL);
        break;
      default:
        LOG_GENERAL(WARNING, "Invalid Sync Type");
        break;
    }

    if (!LOOKUP_NODE_MODE) {
      LOG_GENERAL(INFO, "I am a ds/normal node.");

      if (GETWORK_SERVER_MINE) {
        LOG_GENERAL(INFO, "Starting GetWork Mining Server at http://"
                              << peer.GetPrintableIPAddress() << ":"
                              << GETWORK_SERVER_PORT);

        // start message loop
        if (GetWorkServer::GetInstance().StartServer()) {
          LOG_GENERAL(INFO, "GetWork Mining Server started successfully");
        } else {
          LOG_GENERAL(WARNING, "GetWork Mining Server couldn't start");
        }

      } else {
        LOG_GENERAL(INFO, "GetWork Mining Server not enable")
      }
      // m_mediator.HeartBeatLaunch();
    } else {
      LOG_GENERAL(INFO, "I am a lookup node.");
      m_lookup.SetServerTrue();
    }

    if (LOOKUP_NODE_MODE) {
      m_lookupServerConnector = make_unique<SafeHttpServer>(LOOKUP_RPC_PORT);
      m_lookupServer =
          make_unique<LookupServer>(m_mediator, *m_lookupServerConnector);

      if (m_lookupServer == nullptr) {
        LOG_GENERAL(WARNING, "m_lookupServer NULL");
      } else {
        if (m_lookupServer->StartListening()) {
          LOG_GENERAL(INFO, "API Server started successfully");
          if (ARCHIVAL_LOOKUP) {
            m_lookupServer->StartCollectorThread();
          }
        } else {
          LOG_GENERAL(WARNING, "API Server couldn't start");
        }
      }
    }
    if (ENABLE_STATUS_RPC) {
      m_statusServerConnector =
          make_unique<SafeTcpSocketServer>(IP_TO_BIND, STATUS_RPC_PORT);
      m_statusServer =
          make_unique<StatusServer>(m_mediator, *m_statusServerConnector);
      if (m_statusServer == nullptr) {
        LOG_GENERAL(WARNING, "m_statusServer NULL");
      } else {
        if (m_statusServer->StartListening()) {
          LOG_GENERAL(INFO, "Status Server started successfully");
        } else {
          LOG_GENERAL(WARNING, "Status Server couldn't start");
        }
      }
    }
  };
  DetachedFunction(1, func);
}

Zilliqa::~Zilliqa() {
  pair<bytes, Peer>* message = NULL;
  while (m_msgQueue.pop(message)) {
    delete message;
  }
}

void Zilliqa::Dispatch(pair<bytes, Peer>* message) {
  // LOG_MARKER();

  // Queue message
  if (!m_msgQueue.bounded_push(message)) {
    LOG_GENERAL(WARNING, "Input MsgQueue is full");
  }
}
