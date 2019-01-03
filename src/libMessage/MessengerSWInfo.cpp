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
