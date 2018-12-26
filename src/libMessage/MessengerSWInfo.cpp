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

#include "MessengerSWInfo.h"
#include "libMessage/ZilliqaMessage.pb.h"
#include "libUtils/Logger.h"
#include "libUtils/SWInfo.h"

using namespace std;
using namespace ZilliqaMessage;

template <class T = ProtoSWInfo>
bool SerializeToArray(const T& protoMessage, bytes& dst,
                      const unsigned int offset) {
  if ((offset + protoMessage.ByteSize()) > dst.size()) {
    dst.resize(offset + protoMessage.ByteSize());
  }

  return protoMessage.SerializeToArray(dst.data() + offset,
                                       protoMessage.ByteSize());
}

void SWInfoToProtobuf(const SWInfo& swInfo, ProtoSWInfo& protoSWInfo) {
  protoSWInfo.set_majorversion(swInfo.GetMajorVersion());
  protoSWInfo.set_minorversion(swInfo.GetMinorVersion());
  protoSWInfo.set_fixversion(swInfo.GetFixVersion());
  protoSWInfo.set_upgradeds(swInfo.GetUpgradeDS());
  protoSWInfo.set_commit(swInfo.GetCommit());
}

void ProtobufToSWInfo(const ProtoSWInfo& protoSWInfo, SWInfo& swInfo) {
  swInfo = SWInfo(protoSWInfo.majorversion(), protoSWInfo.minorversion(),
                  protoSWInfo.fixversion(), protoSWInfo.upgradeds(),
                  protoSWInfo.commit());
}

bool MessengerSWInfo::SetSWInfo(bytes& dst, const unsigned int offset,
                                const SWInfo& swInfo) {
  ProtoSWInfo result;

  SWInfoToProtobuf(swInfo, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "SWInfo initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool MessengerSWInfo::GetSWInfo(const bytes& src, const unsigned int offset,
                                SWInfo& swInfo) {
  ProtoSWInfo result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "SWInfo initialization failed.");
    return false;
  }

  ProtobufToSWInfo(result, swInfo);

  return true;
}