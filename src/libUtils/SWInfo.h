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

const std::string VERSION_TAG = "v4.6.1";
const std::string ZILLIQA_BRAND = "Copyright (C) Zilliqa. Version " +
                                  VERSION_TAG + ".  <https://www.zilliqa.com/>";

class SWInfo : public Serializable {
  uint32_t m_zilliqaMajorVersion;
  uint32_t m_zilliqaMinorVersion;
  uint32_t m_zilliqaFixVersion;
  uint64_t m_zilliqaUpgradeDS;
  uint32_t m_zilliqaCommit;
  uint32_t m_scillaMajorVersion;
  uint32_t m_scillaMinorVersion;
  uint32_t m_scillaFixVersion;
  uint64_t m_scillaUpgradeDS;
  uint32_t m_scillaCommit;

 public:
  static const unsigned int SIZE =
      sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) +
      sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t) +
      sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t);

  /// Default constructor for uninitialized version information.
  SWInfo();

  /// Constructor.
  SWInfo(const uint32_t& zilliqaMajorVersion,
         const uint32_t& zilliqaMinorVersion, const uint32_t& zilliqaFixVersion,
         const uint64_t& zilliqaUpgradeDS, const uint32_t& zilliqaCommit,
         const uint32_t& scillaMajorVersion, const uint32_t& scillaMinorVersion,
         const uint32_t& scillaFixVersion, const uint64_t& scillaUpgradeDS,
         const uint32_t& scillaCommit);

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
  const uint32_t& GetZilliqaMajorVersion() const;
  const uint32_t& GetZilliqaMinorVersion() const;
  const uint32_t& GetZilliqaFixVersion() const;
  const uint64_t& GetZilliqaUpgradeDS() const;
  const uint32_t& GetZilliqaCommit() const;
  const uint32_t& GetScillaMajorVersion() const;
  const uint32_t& GetScillaMinorVersion() const;
  const uint32_t& GetScillaFixVersion() const;
  const uint64_t& GetScillaUpgradeDS() const;
  const uint32_t& GetScillaCommit() const;

  static void LogBrand() { std::cout << ZILLIQA_BRAND << std::endl; }

  static void LogBugReport() {
    std::cout << "For bug reporting, please create an issue at "
                 "<https://github.com/Zilliqa/Zilliqa> \n"
              << std::endl;
  }

  static void LogBrandBugReport() {
    LogBrand();
    LogBugReport();
  }

  static bool IsLatestVersion();

  friend std::ostream& operator<<(std::ostream& os, const SWInfo& t);
};

inline std::ostream& operator<<(std::ostream& os, const SWInfo& t) {
  os << "<SWInfo>" << std::endl
     << " m_zilliqaMajorVersion = " << t.m_zilliqaMajorVersion << std::endl
     << " m_zilliqaMinorVersion = " << t.m_zilliqaMinorVersion << std::endl
     << " m_zilliqaFixVersion   = " << t.m_zilliqaFixVersion << std::endl
     << " m_zilliqaUpgradeDS    = " << t.m_zilliqaUpgradeDS << std::endl
     << " m_zilliqaCommit       = " << t.m_zilliqaCommit << std::endl
     << " m_scillaMajorVersion  = " << t.m_scillaMajorVersion << std::endl
     << " m_scillaMinorVersion  = " << t.m_scillaMinorVersion << std::endl
     << " m_scillaFixVersion    = " << t.m_scillaFixVersion << std::endl
     << " m_scillaUpgradeDS     = " << t.m_scillaUpgradeDS << std::endl
     << " m_scillaCommit        = " << t.m_scillaCommit;

  return os;
}

#endif  // __SWINFO_H__
