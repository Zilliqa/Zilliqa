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

#include "SWInfo.h"
#include <curl/curl.h>
#include <json/json.h>
#include "libMessage/MessengerSWInfo.h"
#include "libUtils/Logger.h"

using namespace std;

static const std::string ZILLIQA_RELEASE_TAG_URL(
    "https://api.github.com/repos/Zilliqa/Zilliqa/tags");

SWInfo::SWInfo()
    : m_zilliqaMajorVersion(0),
      m_zilliqaMinorVersion(0),
      m_zilliqaFixVersion(0),
      m_zilliqaUpgradeDS(0),
      m_zilliqaCommit(0),
      m_scillaMajorVersion(0),
      m_scillaMinorVersion(0),
      m_scillaFixVersion(0),
      m_scillaUpgradeDS(0),
      m_scillaCommit(0) {}

SWInfo::SWInfo(const uint32_t& zilliqaMajorVersion,
               const uint32_t& zilliqaMinorVersion,
               const uint32_t& zilliqaFixVersion,
               const uint64_t& zilliqaUpgradeDS, const uint32_t& zilliqaCommit,
               const uint32_t& scillaMajorVersion,
               const uint32_t& scillaMinorVersion,
               const uint32_t& scillaFixVersion,
               const uint64_t& scillaUpgradeDS, const uint32_t& scillaCommit)
    : m_zilliqaMajorVersion(zilliqaMajorVersion),
      m_zilliqaMinorVersion(zilliqaMinorVersion),
      m_zilliqaFixVersion(zilliqaFixVersion),
      m_zilliqaUpgradeDS(zilliqaUpgradeDS),
      m_zilliqaCommit(zilliqaCommit),
      m_scillaMajorVersion(scillaMajorVersion),
      m_scillaMinorVersion(scillaMinorVersion),
      m_scillaFixVersion(scillaFixVersion),
      m_scillaUpgradeDS(scillaUpgradeDS),
      m_scillaCommit(scillaCommit) {}

SWInfo::SWInfo(const SWInfo& src)
    : m_zilliqaMajorVersion(src.m_zilliqaMajorVersion),
      m_zilliqaMinorVersion(src.m_zilliqaMinorVersion),
      m_zilliqaFixVersion(src.m_zilliqaFixVersion),
      m_zilliqaUpgradeDS(src.m_zilliqaUpgradeDS),
      m_zilliqaCommit(src.m_zilliqaCommit),
      m_scillaMajorVersion(src.m_scillaMajorVersion),
      m_scillaMinorVersion(src.m_scillaMinorVersion),
      m_scillaFixVersion(src.m_scillaFixVersion),
      m_scillaUpgradeDS(src.m_scillaUpgradeDS),
      m_scillaCommit(src.m_scillaCommit) {}

SWInfo::~SWInfo(){};

/// Implements the Serialize function inherited from Serializable.
unsigned int SWInfo::Serialize(bytes& dst, unsigned int offset) const {
  // LOG_MARKER();

  if ((offset + SIZE) > dst.size()) {
    dst.resize(offset + SIZE);
  }

  unsigned int curOffset = offset;

  SetNumber<uint32_t>(dst, curOffset, m_zilliqaMajorVersion, sizeof(uint32_t));
  curOffset += sizeof(uint32_t);
  SetNumber<uint32_t>(dst, curOffset, m_zilliqaMinorVersion, sizeof(uint32_t));
  curOffset += sizeof(uint32_t);
  SetNumber<uint32_t>(dst, curOffset, m_zilliqaFixVersion, sizeof(uint32_t));
  curOffset += sizeof(uint32_t);
  SetNumber<uint64_t>(dst, curOffset, m_zilliqaUpgradeDS, sizeof(uint64_t));
  curOffset += sizeof(uint64_t);
  SetNumber<uint32_t>(dst, curOffset, m_zilliqaCommit, sizeof(uint32_t));
  curOffset += sizeof(uint32_t);
  SetNumber<uint32_t>(dst, curOffset, m_scillaMajorVersion, sizeof(uint32_t));
  curOffset += sizeof(uint32_t);
  SetNumber<uint32_t>(dst, curOffset, m_scillaMinorVersion, sizeof(uint32_t));
  curOffset += sizeof(uint32_t);
  SetNumber<uint32_t>(dst, curOffset, m_scillaFixVersion, sizeof(uint32_t));
  curOffset += sizeof(uint32_t);
  SetNumber<uint64_t>(dst, curOffset, m_scillaUpgradeDS, sizeof(uint64_t));
  curOffset += sizeof(uint64_t);
  SetNumber<uint32_t>(dst, curOffset, m_scillaCommit, sizeof(uint32_t));
  curOffset += sizeof(uint32_t);

  return SIZE;
}

/// Implements the Deserialize function inherited from Serializable.
int SWInfo::Deserialize(const bytes& src, unsigned int offset) {
  // LOG_MARKER();

  unsigned int curOffset = offset;

  try {
    m_zilliqaMajorVersion =
        GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    m_zilliqaMinorVersion =
        GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    m_zilliqaFixVersion = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    m_zilliqaUpgradeDS = GetNumber<uint64_t>(src, curOffset, sizeof(uint64_t));
    curOffset += sizeof(uint64_t);
    m_zilliqaCommit = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    m_scillaMajorVersion =
        GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    m_scillaMinorVersion =
        GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    m_scillaFixVersion = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    m_scillaUpgradeDS = GetNumber<uint64_t>(src, curOffset, sizeof(uint64_t));
    curOffset += sizeof(uint64_t);
    m_scillaCommit = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Error with SWInfo::Deserialize." << ' ' << e.what());
    return -1;
  }

  return 0;
}

/// Less-than comparison operator.
bool SWInfo::operator<(const SWInfo& r) const {
  return tie(m_zilliqaMajorVersion, m_zilliqaMinorVersion, m_zilliqaFixVersion,
             m_zilliqaUpgradeDS, m_zilliqaCommit, m_scillaMajorVersion,
             m_scillaMinorVersion, m_scillaFixVersion, m_scillaUpgradeDS,
             m_scillaCommit) <
         tie(r.m_zilliqaMajorVersion, r.m_zilliqaMinorVersion,
             r.m_zilliqaFixVersion, r.m_zilliqaUpgradeDS, r.m_zilliqaCommit,
             r.m_scillaMajorVersion, r.m_scillaMinorVersion,
             r.m_scillaFixVersion, r.m_scillaUpgradeDS, r.m_scillaCommit);
}

/// Greater-than comparison operator.
bool SWInfo::operator>(const SWInfo& r) const { return r < *this; }

/// Equality operator.
bool SWInfo::operator==(const SWInfo& r) const {
  return tie(m_zilliqaMajorVersion, m_zilliqaMinorVersion, m_zilliqaFixVersion,
             m_zilliqaUpgradeDS, m_zilliqaCommit, m_scillaMajorVersion,
             m_scillaMinorVersion, m_scillaFixVersion, m_scillaUpgradeDS,
             m_scillaCommit) ==
         tie(r.m_zilliqaMajorVersion, r.m_zilliqaMinorVersion,
             r.m_zilliqaFixVersion, r.m_zilliqaUpgradeDS, r.m_zilliqaCommit,
             r.m_scillaMajorVersion, r.m_scillaMinorVersion,
             r.m_scillaFixVersion, r.m_scillaUpgradeDS, r.m_scillaCommit);
}

/// Unequality operator.
bool SWInfo::operator!=(const SWInfo& r) const { return !(*this == r); }

/// Getters.
const uint32_t& SWInfo::GetZilliqaMajorVersion() const {
  return m_zilliqaMajorVersion;
}

const uint32_t& SWInfo::GetZilliqaMinorVersion() const {
  return m_zilliqaMinorVersion;
}

const uint32_t& SWInfo::GetZilliqaFixVersion() const {
  return m_zilliqaFixVersion;
}

const uint64_t& SWInfo::GetZilliqaUpgradeDS() const {
  return m_zilliqaUpgradeDS;
}

const uint32_t& SWInfo::GetZilliqaCommit() const { return m_zilliqaCommit; }

const uint32_t& SWInfo::GetScillaMajorVersion() const {
  return m_scillaMajorVersion;
}

const uint32_t& SWInfo::GetScillaMinorVersion() const {
  return m_scillaMinorVersion;
}

const uint32_t& SWInfo::GetScillaFixVersion() const {
  return m_scillaFixVersion;
}

const uint64_t& SWInfo::GetScillaUpgradeDS() const { return m_scillaUpgradeDS; }

const uint32_t& SWInfo::GetScillaCommit() const { return m_scillaCommit; }

static size_t WriteString(void* contents, size_t size, size_t nmemb,
                          void* userp) {
  ((string*)userp)->append((char*)contents, size * nmemb);
  return size * nmemb;
}

bool SWInfo::IsLatestVersion() {
  string curlRes;
  auto curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_URL, ZILLIQA_RELEASE_TAG_URL.c_str());
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "zilliqa");
  curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteString);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curlRes);
  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    LOG_GENERAL(WARNING,
                "curl_easy_perform() failed to get latest release tag");
    return false;
  }

  try {
    Json::Value jsonValue;
    std::string errors;
    Json::CharReaderBuilder builder;
    auto reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());
    if (!reader->parse(curlRes.c_str(), curlRes.c_str() + curlRes.size(),
                       &jsonValue, &errors)) {
      LOG_GENERAL(WARNING,
                  "Failed to parse return result to json: " << curlRes);
      LOG_GENERAL(WARNING, "Error: " << errors);
      return false;
    }

    Json::Value jsonLatestTag = jsonValue[0];
    std::string latestTag = jsonLatestTag["name"].asCString();

    LOG_GENERAL(INFO, "The latest software version: " << latestTag);
    if (VERSION_TAG < latestTag) {
      return false;
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Failed to parse tag information, exception: " << e.what());
    return false;
  }

  return true;
}
