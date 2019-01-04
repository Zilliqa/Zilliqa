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

#ifndef __SWINFO_H__
#define __SWINFO_H__

#include <stdint.h>
#include <iostream>
#include "common/Serializable.h"

class SWInfo : public Serializable {
  uint32_t m_majorVersion;
  uint32_t m_minorVersion;
  uint32_t m_fixVersion;
  uint64_t m_upgradeDS;
  uint32_t m_commit;

 public:
  static const unsigned int SIZE = sizeof(uint32_t) + sizeof(uint32_t) +
                                   sizeof(uint32_t) + sizeof(uint64_t) +
                                   sizeof(uint32_t);

  /// Default constructor for uninitialized version information.
  SWInfo();

  /// Constructor.
  SWInfo(const uint32_t& majorVersion, const uint32_t& minorVersion,
         const uint32_t& fixVersion, const uint64_t& upgradeDS,
         const uint32_t& commit);

  /// Destructor.
  ~SWInfo();

  /// Copy constructor.
  SWInfo(const SWInfo&);

  /// Implements the Serialize function inherited from Serializable.
  unsigned int Serialize(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  int Deserialize(const bytes& src, unsigned int offset);

  /// Less-than comparison operator.
  bool operator<(const SWInfo& r) const;

  /// Greater-than comparison operator.
  bool operator>(const SWInfo& r) const;

  /// Equality operator.
  bool operator==(const SWInfo& r) const;

  /// Unequality operator.
  bool operator!=(const SWInfo& r) const;

  /// Getters.
  const uint32_t& GetMajorVersion() const;
  const uint32_t& GetMinorVersion() const;
  const uint32_t& GetFixVersion() const;
  const uint64_t& GetUpgradeDS() const;
  const uint32_t& GetCommit() const;

  friend std::ostream& operator<<(std::ostream& os, const SWInfo& t);
};

inline std::ostream& operator<<(std::ostream& os, const SWInfo& t) {
  os << "<SWInfo>" << std::endl
     << "m_majorVersion : " << t.m_majorVersion << std::endl
     << "m_minorVersion : " << t.m_minorVersion << std::endl
     << "m_fixVersion : " << t.m_fixVersion << std::endl
     << "m_upgradeDS : " << t.m_upgradeDS << std::endl
     << "m_commit : " << t.m_commit;

  return os;
}

#endif  // __SWINFO_H__
