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

#include "VCBlockHeader.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

VCBlockHeader::VCBlockHeader(
    uint64_t vieWChangeDSEpochNo, uint64_t viewChangeEpochNo,
    unsigned char viewChangeState, const Peer& candidateLeaderNetworkInfo,
    const PubKey& candidateLeaderPubKey, uint32_t vcCounter,
    const VectorOfNode& faultyLeaders, uint32_t version,
    const CommitteeHash& committeeHash, const BlockHash& prevHash)
    : BlockHeaderBase(version, committeeHash, prevHash),
      m_VieWChangeDSEpochNo(vieWChangeDSEpochNo),
      m_VieWChangeEpochNo(viewChangeEpochNo),
      m_ViewChangeState(viewChangeState),
      m_CandidateLeaderNetworkInfo(candidateLeaderNetworkInfo),
      m_CandidateLeaderPubKey(candidateLeaderPubKey),
      m_VCCounter(vcCounter),
      m_FaultyLeaders(faultyLeaders) {}

bool VCBlockHeader::Serialize(zbytes& dst, unsigned int offset) const {
  if (!Messenger::SetVCBlockHeader(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetVCBlockHeader failed.");
    return false;
  }

  return true;
}

bool VCBlockHeader::Deserialize(const zbytes& src, unsigned int offset) {
  if (!Messenger::GetVCBlockHeader(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetVCBlockHeader failed.");
    return false;
  }

  return true;
}

bool VCBlockHeader::Deserialize(const string& src, unsigned int offset) {
  if (!Messenger::GetVCBlockHeader(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetVCBlockHeader failed.");
    return false;
  }

  return true;
}

bool VCBlockHeader::operator==(const VCBlockHeader& header) const {
  return BlockHeaderBase::operator==(header) &&
         (std::tie(m_VieWChangeDSEpochNo, m_VieWChangeEpochNo,
                   m_ViewChangeState, m_CandidateLeaderNetworkInfo,
                   m_CandidateLeaderPubKey, m_VCCounter, m_FaultyLeaders) ==
          std::tie(header.m_VieWChangeDSEpochNo, header.m_VieWChangeEpochNo,
                   header.m_ViewChangeState,
                   header.m_CandidateLeaderNetworkInfo,
                   header.m_CandidateLeaderPubKey, header.m_VCCounter,
                   header.m_FaultyLeaders));
}

std::ostream& operator<<(std::ostream& os, const VCBlockHeader& t) {
  const BlockHeaderBase& blockHeaderBase(t);

  os << blockHeaderBase << std::endl
     << "<VCBlockHeader>" << std::endl
     << " VieWChangeDSEpochNo        = " << t.GetViewChangeDSEpochNo()
     << std::endl
     << " VieWChangeEpochNo          = " << t.GetViewChangeEpochNo()
     << std::endl
     << " ViewChangeState            = " << t.GetViewChangeState() << std::endl
     << " CandidateLeaderNetworkInfo = " << t.GetCandidateLeaderNetworkInfo()
     << std::endl
     << " CandidateLeaderPubKey      = " << t.GetCandidateLeaderPubKey()
     << std::endl
     << " VCCounter                  = " << t.GetViewChangeCounter()
     << std::endl;
  for (const auto& node : t.GetFaultyLeaders()) {
    os << " FaultyLeader                 = " << node.first << " " << node.second
       << std::endl;
  }
  return os;
}
