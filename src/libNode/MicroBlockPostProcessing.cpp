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

#include <array>
#include <chrono>
#include <functional>
#include <thread>

#include "Node.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/AccountStore/AccountStore.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/TimeUtils.h"

using namespace std;
using namespace boost::multiprecision;

namespace zil {
namespace local {

class MicroBlockPostProcessingVariables {
  int errorsMissingTx = 0;
  int consensusErrorCode = -1;
  int microblockConsensusMessages = 0;
  int microblockConsensusFailedBadly = 0;

 public:
  std::unique_ptr<Z_I64GAUGE> temp;

  void SetConsensusErrorCode(int code) {
    Init();
    consensusErrorCode = code;
  }

  void AddErrorsMissingTx(int missingTx) {
    Init();
    errorsMissingTx += missingTx;
  }

  void AddMicroblockConsensusMessages(int count) {
    Init();
    microblockConsensusMessages += count;
  }

  void AddMicroblockConsensusFailedBadly(int count) {
    Init();
    microblockConsensusFailedBadly += count;
  }

  void Init() {
    if (!temp) {
      temp = std::make_unique<Z_I64GAUGE>(Z_FL::BLOCKS, "consensus.gauge",
                                          "Consensus", "calls", true);

      temp->SetCallback([this](auto&& result) {
        result.Set(consensusErrorCode, {{"counter", "ConsensusErrorCode"}});
        result.Set(errorsMissingTx, {{"counter", "ErrorsMissingTx"}});
        result.Set(microblockConsensusMessages,
                   {{"counter", "MicroblockConsensusMessages"}});
        result.Set(microblockConsensusFailedBadly,
                   {{"counter", "MicroblockConsensusFailedBadly"}});
      });
    }
  }
};

static MicroBlockPostProcessingVariables variables{};

}  // namespace local
}  // namespace zil

bool Node::ComposeMicroBlockMessageForSender(zbytes& microblock_message) const {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ComposeMicroBlockMessageForSender not expected to be "
                "called from LookUp node.");
    return false;
  }

  microblock_message.clear();

  microblock_message = {MessageType::DIRECTORY,
                        DSInstructionType::MICROBLOCKSUBMISSION};
  zbytes stateDelta;
  AccountStore::GetInstance().GetSerializedDelta(stateDelta);

  if (!Messenger::SetDSMicroBlockSubmission(
          microblock_message, MessageOffset::BODY,
          DirectoryService::SUBMITMICROBLOCKTYPE::SHARDMICROBLOCK,
          m_mediator.m_currentEpochNum, {*m_microblock}, {stateDelta},
          m_mediator.m_selfKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetDSMicroBlockSubmission failed.");
    return false;
  }

  return true;
}

bool Node::ProcessMicroBlockConsensus(
    const zbytes& /* message */, unsigned int /* offset */,
    const Peer& /* from */, [[gnu::unused]] const unsigned char& startByte) {
  LOG_GENERAL(WARNING,
              "Node::ProcessMicroBlockConsensus not expected to be "
              "called from desharded config.");
  return false;
}

void Node::CleanMicroblockConsensusBuffer() {
  lock_guard<mutex> g(m_mutexMicroBlockConsensusBuffer);
  m_microBlockConsensusBuffer.clear();
}
