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

#include "SWInfo.h"
#include "libMessage/MessengerSWInfo.h"
#include "libUtils/Logger.h"

using namespace std;

SWInfo::SWInfo()
    : m_majorVersion(0),
      m_minorVersion(0),
      m_fixVersion(0),
      m_upgradeDS(0),
      m_commit(0) {}

SWInfo::SWInfo(const uint32_t& majorVersion, const uint32_t& minorVersion,
               const uint32_t& fixVersion, const uint64_t& upgradeDS,
               const uint32_t& commit)
    : m_majorVersion(majorVersion),
      m_minorVersion(minorVersion),
      m_fixVersion(fixVersion),
      m_upgradeDS(upgradeDS),
      m_commit(commit) {}

SWInfo::SWInfo(const SWInfo& src)
    : m_majorVersion(src.m_majorVersion),
      m_minorVersion(src.m_minorVersion),
      m_fixVersion(src.m_fixVersion),
      m_upgradeDS(src.m_upgradeDS),
      m_commit(src.m_commit) {}

SWInfo::~SWInfo(){};

/// Implements the Serialize function inherited from Serializable.
bool SWInfo::Serialize(std::vector<unsigned char>& dst,
                       unsigned int offset) const {
  if (!MessengerSWInfo::SetSWInfo(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "MessengerSWInfo::SetSWInfo failed.");
    return false;
  }

  return true;
}

/// Implements the Deserialize function inherited from Serializable.
bool SWInfo::Deserialize(const std::vector<unsigned char>& src,
                         unsigned int offset) {
  if (!MessengerSWInfo::GetSWInfo(src, offset, *this)) {
    LOG_GENERAL(WARNING, "MessengerSWInfo::GetSWInfo failed.");
    return false;
  }

  return true;
}

/// Less-than comparison operator.
bool SWInfo::operator<(const SWInfo& r) const {
  return tie(m_majorVersion, m_minorVersion, m_fixVersion, m_upgradeDS,
             m_commit) < tie(r.m_majorVersion, r.m_minorVersion, r.m_fixVersion,
                             r.m_upgradeDS, r.m_commit);
}

/// Greater-than comparison operator.
bool SWInfo::operator>(const SWInfo& r) const { return r < *this; }

/// Equality operator.
bool SWInfo::operator==(const SWInfo& r) const {
  return tie(m_majorVersion, m_minorVersion, m_fixVersion, m_upgradeDS,
             m_commit) == tie(r.m_majorVersion, r.m_minorVersion,
                              r.m_fixVersion, r.m_upgradeDS, r.m_commit);
}

/// Unequality operator.
bool SWInfo::operator!=(const SWInfo& r) const { return !(*this == r); }

/// Getters.
const uint32_t& SWInfo::GetMajorVersion() const { return m_majorVersion; };
const uint32_t& SWInfo::GetMinorVersion() const { return m_minorVersion; };
const uint32_t& SWInfo::GetFixVersion() const { return m_fixVersion; };
const uint64_t& SWInfo::GetUpgradeDS() const { return m_upgradeDS; };
const uint32_t& SWInfo::GetCommit() const { return m_commit; };
