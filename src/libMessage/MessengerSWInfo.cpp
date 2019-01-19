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

inline bool CheckRequiredFieldsProtoSWInfo(const ProtoSWInfo& protoSWInfo) {
  return protoSWInfo.has_zilliqamajorversion() &&
         protoSWInfo.has_zilliqaminorversion() &&
         protoSWInfo.has_zilliqafixversion() &&
         protoSWInfo.has_zilliqaupgradeds() &&
         protoSWInfo.has_zilliqacommit() &&
         protoSWInfo.has_scillamajorversion() &&
         protoSWInfo.has_scillaminorversion() &&
         protoSWInfo.has_scillafixversion() &&
         protoSWInfo.has_scillaupgradeds() && protoSWInfo.has_scillacommit();
}

void SWInfoToProtobuf(const SWInfo& swInfo, ProtoSWInfo& protoSWInfo) {
  protoSWInfo.set_zilliqamajorversion(swInfo.GetZilliqaMajorVersion());
  protoSWInfo.set_zilliqaminorversion(swInfo.GetZilliqaMinorVersion());
  protoSWInfo.set_zilliqafixversion(swInfo.GetZilliqaFixVersion());
  protoSWInfo.set_zilliqaupgradeds(swInfo.GetZilliqaUpgradeDS());
  protoSWInfo.set_zilliqacommit(swInfo.GetZilliqaCommit());
  protoSWInfo.set_scillamajorversion(swInfo.GetScillaMajorVersion());
  protoSWInfo.set_scillaminorversion(swInfo.GetScillaMinorVersion());
  protoSWInfo.set_scillafixversion(swInfo.GetScillaFixVersion());
  protoSWInfo.set_scillaupgradeds(swInfo.GetScillaUpgradeDS());
  protoSWInfo.set_scillacommit(swInfo.GetScillaCommit());
}

bool ProtobufToSWInfo(const ProtoSWInfo& protoSWInfo, SWInfo& swInfo) {
  if (!CheckRequiredFieldsProtoSWInfo(protoSWInfo)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoSWInfo failed");
    return false;
  }
  swInfo = SWInfo(
      protoSWInfo.zilliqamajorversion(), protoSWInfo.zilliqaminorversion(),
      protoSWInfo.zilliqafixversion(), protoSWInfo.zilliqaupgradeds(),
      protoSWInfo.zilliqacommit(), protoSWInfo.scillamajorversion(),
      protoSWInfo.scillaminorversion(), protoSWInfo.scillafixversion(),
      protoSWInfo.scillaupgradeds(), protoSWInfo.scillacommit());
  return false;
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

  return ProtobufToSWInfo(result, swInfo);
}
