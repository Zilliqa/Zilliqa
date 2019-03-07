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

#include "UpgradeManager.h"
#include <sys/wait.h>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/tokenizer.hpp>
#include "libCrypto/MultiSig.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"

using namespace std;

#define USER_AGENT "Zilliqa"
#define DOWNLOAD_FOLDER "download"
#define UPGRADE_HOST                                                      \
  string(string("https://api.github.com/repos/") + UPGRADE_HOST_ACCOUNT + \
         "/" + UPGRADE_HOST_REPO + "/releases/latest")

namespace {

const string dsNodePubProp = "pubk";
const string publicKeyProp = "publicKey";
const string signatureProp = "signature";
struct PTree {
  static boost::property_tree::ptree& GetInstance() {
    static boost::property_tree::ptree pt;
    read_xml(dsNodeFile.c_str(), pt);
    return pt;
  }
  PTree() = delete;
  ~PTree() = delete;
};

const vector<string> ReadDSCommFromFile() {
  auto pt = PTree::GetInstance();
  std::vector<std::string> result;
  for (auto& pubk : pt.get_child("dsnodes")) {
    if (pubk.first == dsNodePubProp) {
      result.emplace_back(pubk.second.data());
    }
  }
  return result;
}

const string ReadDSCommFile(string propName) {
  auto pt = PTree::GetInstance();
  return pt.get<string>(propName);
}
}  // namespace

UpgradeManager::UpgradeManager() {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  m_curl = curl_easy_init();

  if (!m_curl) {
    LOG_GENERAL(WARNING, "curl initialization fail!");
    curl_global_cleanup();
  }

  boost::filesystem::create_directories(DOWNLOAD_FOLDER);
}

UpgradeManager::~UpgradeManager() {
  if (m_curl) {
    curl_easy_cleanup(m_curl);
  }

  curl_global_cleanup();
}

UpgradeManager& UpgradeManager::GetInstance() {
  static UpgradeManager um;
  return um;
}

static size_t WriteString(void* contents, size_t size, size_t nmemb,
                          void* userp) {
  ((string*)userp)->append((char*)contents, size * nmemb);
  return size * nmemb;
}

string UpgradeManager::DownloadFile(const char* fileTail,
                                    const char* releaseUrl) {
  LOG_MARKER();
  if (!m_curl) {
    LOG_GENERAL(WARNING, "Cannot perform any curl operation!");
    return "";
  }

  string curlRes;
  curl_easy_reset(m_curl);
  curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(m_curl, CURLOPT_URL,
                   releaseUrl ? releaseUrl : UPGRADE_HOST.c_str());
  curl_easy_setopt(m_curl, CURLOPT_USERAGENT, USER_AGENT);
  curl_easy_setopt(m_curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
  curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, WriteString);
  curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &curlRes);
  CURLcode res = curl_easy_perform(m_curl);

  if (res != CURLE_OK) {
    LOG_GENERAL(WARNING,
                "curl_easy_perform() failed to get latest release "
                "information from url ["
                    << (releaseUrl ? releaseUrl : UPGRADE_HOST)
                    << "]: " << curl_easy_strerror(res));
    return "";
  }

  LOG_GENERAL(INFO, "curlRes: = " << curlRes);

  int find = 0;
  string cur;
  vector<string> downloadFilePaths;
  boost::char_separator<char> sep(",\"");
  boost::tokenizer<boost::char_separator<char>> tokens(curlRes, sep);
  for (boost::tokenizer<boost::char_separator<char>>::iterator tok_iter =
           tokens.begin();
       tok_iter != tokens.end(); ++tok_iter) {
    if (1 == find) {
      ++find;
      continue;
    }

    if (2 == find) {
      find = 0;
      downloadFilePaths.emplace_back(*tok_iter);
      continue;
    }

    cur = *tok_iter;

    if (cur == "browser_download_url") {
      find = 1;
      continue;
    }
  }

  string downloadFilePath;

  for (auto s : downloadFilePaths) {
    if (string::npos != s.rfind(fileTail)) {
      downloadFilePath = s;
      break;
    }
  }

  LOG_GENERAL(INFO, "downloadFilePath: " << downloadFilePath);
  string fileName = string(DOWNLOAD_FOLDER) + "/" +
                    downloadFilePath.substr(downloadFilePath.rfind('/') + 1);

  /// Get the redirection url (if applicable)
  curl_easy_reset(m_curl);
  curl_easy_setopt(m_curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
  curl_easy_setopt(m_curl, CURLOPT_URL, downloadFilePath.data());
  res = curl_easy_perform(m_curl);

  if (res != CURLE_OK) {
    LOG_GENERAL(INFO,
                "curl_easy_perform() failed to get redirect url from url ["
                    << downloadFilePath << "]: " << curl_easy_strerror(res));
    return "";
  }

  long response_code;
  res = curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &response_code);

  if ((res != CURLE_OK) || ((response_code / 100) == 3)) {
    char* location = nullptr;
    res = curl_easy_getinfo(m_curl, CURLINFO_REDIRECT_URL, &location);

    if ((res == CURLE_OK) && location) {
      downloadFilePath = location;
    }
  }

  /// Download the file
  curl_easy_reset(m_curl);
  curl_easy_setopt(m_curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
  curl_easy_setopt(m_curl, CURLOPT_URL, downloadFilePath.data());
  curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 1L);
  unique_ptr<FILE, decltype(&fclose)> fp(fopen(fileName.data(), "wb"), &fclose);

  if (!fp) {
    return "";
  }

  curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, fp.get());
  res = curl_easy_perform(m_curl);

  if (res != CURLE_OK) {
    LOG_GENERAL(INFO, "curl_easy_perform() failed to download file from url["
                          << downloadFilePath
                          << "]: " << curl_easy_strerror(res));
    return "";
  }

  return fileName;
}

bool UpgradeManager::LoadInitialDS(vector<PubKey>& initialDSCommittee) {
  string downloadUrl = "";
  try {
    if (GET_INITIAL_DS_FROM_REPO) {
      string dsnodeFile = DownloadFile(dsNodeFile.data(), downloadUrl.c_str());
      boost::filesystem::copy_file(
          dsnodeFile, dsNodeFile,
          boost::filesystem::copy_option::overwrite_if_exists);
      auto pt = PTree::GetInstance();

      vector<std::string> tempDsComm_string{ReadDSCommFromFile()};
      initialDSCommittee.clear();
      for (auto ds_string : tempDsComm_string) {
        bytes pubkeyBytes;
        if (!DataConversion::HexStrToUint8Vec(ds_string, pubkeyBytes)) {
          LOG_GENERAL(WARNING,
                      "error loading "
                          << ds_string
                          << " using HexStrToUint8Vec(). Not a hex str");
          continue;
        }
        initialDSCommittee.push_back(PubKey(pubkeyBytes, 0));
      }

      bytes message;
      unsigned int curr_offset = 0;
      for (auto& dsKey : initialDSCommittee) {
        dsKey.Serialize(message, curr_offset);
        curr_offset += PUB_KEY_SIZE;
      }

      string sig_str = ReadDSCommFile(signatureProp);
      string pubKey_str = ReadDSCommFile(publicKeyProp);

      bytes pubkeyBytes;
      if (!DataConversion::HexStrToUint8Vec(pubKey_str, pubkeyBytes)) {
        return false;
      }
      PubKey pubKey(pubkeyBytes, 0);

      bytes sigBytes;
      if (!DataConversion::HexStrToUint8Vec(sig_str, sigBytes)) {
        return false;
      }
      Signature sig(sigBytes, 0);

      if (!Schnorr::GetInstance().Verify(message, sig, pubKey)) {
        LOG_GENERAL(WARNING, "Unable to verify file");
        return false;
      }
      return true;

    } else {
      vector<std::string> tempDsComm_string{ReadDSCommFromFile()};
      initialDSCommittee.clear();
      for (auto ds_string : tempDsComm_string) {
        bytes pubkeyBytes;
        if (!DataConversion::HexStrToUint8Vec(ds_string, pubkeyBytes)) {
          return false;
        }
        initialDSCommittee.push_back(PubKey(pubkeyBytes, 0));
      }
    }

    return true;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, e.what());
    return false;
  }
}
