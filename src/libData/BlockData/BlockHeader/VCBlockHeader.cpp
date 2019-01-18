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

VCBlockHeader::VCBlockHeader()
    : m_VieWChangeDSEpochNo((uint64_t)-1),
      m_VieWChangeEpochNo((uint64_t)-1),
      m_ViewChangeState(0),
      m_CandidateLeaderNetworkInfo(),
      m_CandidateLeaderPubKey(),
      m_VCCounter(0),
      m_FaultyLeaders() {}

VCBlockHeader::VCBlockHeader(const bytes& src, unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(INFO, "Error. We failed to initialize VCBlockHeader.");
  }
}

VCBlockHeader::VCBlockHeader(
    const uint64_t& vieWChangeDSEpochNo, const uint64_t& viewChangeEpochNo,
    const unsigned char viewChangeState, const Peer& candidateLeaderNetworkInfo,
    const PubKey& candidateLeaderPubKey, const uint32_t vcCounter,
    const VectorOfNode& faultyLeaders, const uint32_t version,
    const CommitteeHash& committeeHash, const BlockHash& prevHash)
    : BlockHeaderBase(version, committeeHash, prevHash),
      m_VieWChangeDSEpochNo(vieWChangeDSEpochNo),
      m_VieWChangeEpochNo(viewChangeEpochNo),
      m_ViewChangeState(viewChangeState),
      m_CandidateLeaderNetworkInfo(candidateLeaderNetworkInfo),
      m_CandidateLeaderPubKey(candidateLeaderPubKey),
      m_VCCounter(vcCounter),
      m_FaultyLeaders(faultyLeaders) {}

bool VCBlockHeader::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetVCBlockHeader(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetVCBlockHeader failed.");
    return false;
  }

  return true;
}

bool VCBlockHeader::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetVCBlockHeader(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetVCBlockHeader failed.");
    return false;
  }

  return true;
}

const uint64_t& VCBlockHeader::GetViewChangeDSEpochNo() const {
  return m_VieWChangeDSEpochNo;
}

const uint64_t& VCBlockHeader::GetViewChangeEpochNo() const {
  return m_VieWChangeEpochNo;
}

unsigned char VCBlockHeader::GetViewChangeState() const {
  return m_ViewChangeState;
}

const Peer& VCBlockHeader::GetCandidateLeaderNetworkInfo() const {
  return m_CandidateLeaderNetworkInfo;
}

const PubKey& VCBlockHeader::GetCandidateLeaderPubKey() const {
  return m_CandidateLeaderPubKey;
}

uint32_t VCBlockHeader::GetViewChangeCounter() const { return m_VCCounter; }

const VectorOfNode& VCBlockHeader::GetFaultyLeaders() const {
  return m_FaultyLeaders;
};

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

bool VCBlockHeader::operator<(const VCBlockHeader& header) const {
  // To compare, first they must be of identical epochno and state
  return BlockHeaderBase::operator==(header) &&
         (std::tie(m_VieWChangeDSEpochNo, m_VieWChangeEpochNo,
                   m_ViewChangeState) == std::tie(header.m_VieWChangeDSEpochNo,
                                                  header.m_VieWChangeEpochNo,
                                                  header.m_ViewChangeState)) &&
         (m_VCCounter < header.m_VCCounter);
}

bool VCBlockHeader::operator>(const VCBlockHeader& header) const {
  return header < *this;
}
