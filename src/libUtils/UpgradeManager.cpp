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
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/tokenizer.hpp>
#include "libCrypto/Schnorr.h"
#include "libUtils/Logger.h"
using namespace std;

#define USER_AGENT "Zilliqa"
#define DOWNLOAD_FOLDER "download"
#define VERSION_FILE_NAME "VERSION"
#define PUBLIC_KEY_FILE_NAME "pubKeyFile"
#define CONSTANT_FILE_NAME "constants.xml"
#define CONSTANT_LOOKUP_FILE_NAME "constants.xml_lookup"
#define CONSTANT_ARCHIVAL_FILE_NAME "constants.xml_archival"
#define PUBLIC_KEY_LENGTH 66
#define PACKAGE_FILE_EXTENSION "deb"
#define UPGRADE_HOST                                                      \
  string(string("https://api.github.com/repos/") + UPGRADE_HOST_ACCOUNT + \
         "/" + UPGRADE_HOST_REPO + "/releases/latest")

const unsigned int TERMINATION_COUNTDOWN_OFFSET_SHARD = 0;
const unsigned int TERMINATION_COUNTDOWN_OFFSET_DS_BACKUP = 1;
const unsigned int TERMINATION_COUNTDOWN_OFFSET_DS_LEADER = 2;
const unsigned int TERMINATION_COUNTDOWN_OFFSET_LOOKUP = 3;

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
  if (!m_curl) {
    LOG_GENERAL(WARNING, "Cannot perform any curl operation!");
    return "";
  }

  string curlRes;
  curl_easy_reset(m_curl);
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

bool UpgradeManager::HasNewSW() {
  LOG_MARKER();

  string pubKeyFileName = DownloadFile(PUBLIC_KEY_FILE_NAME);

  if (pubKeyFileName.empty()) {
    LOG_GENERAL(INFO, "Cannot download public key file!");
    return false;
  }

  LOG_GENERAL(INFO, "public key file has been downloaded successfully.");

  string versionName = DownloadFile(VERSION_FILE_NAME);

  if (versionName.empty()) {
    LOG_GENERAL(INFO, "Cannot download version file!");
    return false;
  }

  LOG_GENERAL(INFO, "Version file has been downloaded successfully.");

  vector<PubKey> pubKeys;
  {
    fstream pubKeyFile(pubKeyFileName, ios::in);
    string pubKey;

    while (getline(pubKeyFile, pubKey) && PUBLIC_KEY_LENGTH == pubKey.size()) {
      pubKeys.emplace_back(DataConversion::HexStrToUint8Vec(pubKey), 0);
    }
  }

  LOG_GENERAL(INFO, "Parsing public key file completed.");

  string shaStr, sigStr;
  {
    fstream versionFile(versionName, ios::in);
    int line_no = 0;

    /// Read SHA-256 hash
    while (line_no != 14 && getline(versionFile, shaStr)) {
      ++line_no;
    }

    /// Read signature
    while (line_no != 16 && getline(versionFile, sigStr)) {
      ++line_no;
    }
  }

  LOG_GENERAL(INFO, "Parsing version file completed.");

  const bytes sha = DataConversion::HexStrToUint8Vec(shaStr);
  const unsigned int len = sigStr.size() / pubKeys.size();
  vector<Signature> mutliSig;

  for (unsigned int i = 0; i < pubKeys.size(); ++i) {
    mutliSig.emplace_back(
        DataConversion::HexStrToUint8Vec(sigStr.substr(i * len, len)), 0);
  }

  /// Multi-sig verification
  for (unsigned int i = 0; i < pubKeys.size(); ++i) {
    if (!Schnorr::GetInstance().Verify(sha, mutliSig.at(i), pubKeys.at(i))) {
      LOG_GENERAL(WARNING, "Multisig verification failed!");
      return false;
    }
  }

  return m_latestSHA != sha;
}

bool UpgradeManager::DownloadSW() {
  LOG_MARKER();
  lock_guard<mutex> guard(m_downloadMutex);
  string versionName = DownloadFile(VERSION_FILE_NAME);

  if (versionName.empty()) {
    LOG_GENERAL(WARNING, "Cannot download version file!");
    return false;
  }

  LOG_GENERAL(INFO, "Version file has been downloaded successfully.");

  m_constantFileName = DownloadFile(CONSTANT_FILE_NAME);

  if (m_constantFileName.empty()) {
    LOG_GENERAL(WARNING, "Cannot download constant file!");
    return false;
  }

  m_constantLookupFileName = DownloadFile(CONSTANT_LOOKUP_FILE_NAME);

  if (m_constantLookupFileName.empty()) {
    LOG_GENERAL(WARNING, "Cannot download constant lookup file!");
    return false;
  }

  m_constantArchivalFileName = DownloadFile(CONSTANT_ARCHIVAL_FILE_NAME);

  if (m_constantArchivalFileName.empty()) {
    LOG_GENERAL(WARNING, "Cannot download constant archival file!");
    return false;
  }

  LOG_GENERAL(INFO, "Constant file has been downloaded successfully.");

  m_packageFileName = DownloadFile(PACKAGE_FILE_EXTENSION);

  if (m_packageFileName.empty()) {
    LOG_GENERAL(WARNING, "Cannot download package (.deb) file!");
    return false;
  }

  LOG_GENERAL(INFO, "Package (.deb) file has been downloaded successfully.");

  uint32_t major, minor, fix, commit;
  uint64_t upgradeDS;
  string sha;

  try {
    fstream versionFile(versionName, ios::in);
    int line_no = 0;
    string line;

    /// Read major version
    while (line_no != 2 && getline(versionFile, line)) {
      ++line_no;
    }

    major = stoul(line);

    /// Read minor version
    while (line_no != 4 && getline(versionFile, line)) {
      ++line_no;
    }

    minor = stoul(line);

    /// Read fix version
    while (line_no != 6 && getline(versionFile, line)) {
      ++line_no;
    }

    fix = stoul(line);

    /// Read expected DS epoch
    while (line_no != 8 && getline(versionFile, line)) {
      ++line_no;
    }

    upgradeDS = stoull(line);

    /// Read Git commit ID
    while (line_no != 12 && getline(versionFile, line)) {
      ++line_no;
    }

    commit = stoul(line, nullptr, 16);

    /// Read SHA-256 hash
    while (line_no != 14 && getline(versionFile, sha)) {
      ++line_no;
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Cannot parse " << VERSION_FILE_NAME << ": " << ' '
                                         << e.what());
    return false;
  }

  /// Verify SHA-256 checksum of .deb file
  string downloadSha;
  {
    fstream debFile(m_packageFileName, ios::in);

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    bytes vec((istreambuf_iterator<char>(debFile)),
              (istreambuf_iterator<char>()));
    sha2.Update(vec, 0, vec.size());
    bytes output = sha2.Finalize();
    downloadSha = DataConversion::Uint8VecToHexStr(output);
  }

  if (sha != downloadSha) {
    LOG_GENERAL(WARNING, "SHA-256 checksum of .deb file mismatch. Expected: "
                             << sha << " Actual: " << downloadSha);
    return false;
  }

  m_latestSWInfo = make_shared<SWInfo>(major, minor, fix, upgradeDS, commit);
  m_latestSHA = DataConversion::HexStrToUint8Vec(sha);
  return true;
}

bool UpgradeManager::ReplaceNode(Mediator& mediator) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(INFO, "Lookup node, upgrade after "
                          << TERMINATION_COUNTDOWN_IN_SECONDS +
                                 TERMINATION_COUNTDOWN_OFFSET_LOOKUP
                          << " seconds...");
    this_thread::sleep_for(
        chrono::seconds(TERMINATION_COUNTDOWN_IN_SECONDS +
                        TERMINATION_COUNTDOWN_OFFSET_LOOKUP));

    BlockStorage::GetBlockStorage().PutMetadata(MetaType::DSINCOMPLETED, {'0'});
  } else {
    if (DirectoryService::IDLE == mediator.m_ds->m_mode) {
      LOG_GENERAL(INFO, "Shard node, upgrade after "
                            << TERMINATION_COUNTDOWN_IN_SECONDS +
                                   TERMINATION_COUNTDOWN_OFFSET_SHARD
                            << " seconds...");
      this_thread::sleep_for(
          chrono::seconds(TERMINATION_COUNTDOWN_IN_SECONDS +
                          TERMINATION_COUNTDOWN_OFFSET_SHARD));
    } else if (DirectoryService::BACKUP_DS == mediator.m_ds->m_mode) {
      LOG_GENERAL(INFO, "DS backup node, upgrade after "
                            << TERMINATION_COUNTDOWN_IN_SECONDS +
                                   TERMINATION_COUNTDOWN_OFFSET_DS_BACKUP
                            << " seconds...");
      this_thread::sleep_for(
          chrono::seconds(TERMINATION_COUNTDOWN_IN_SECONDS +
                          TERMINATION_COUNTDOWN_OFFSET_DS_BACKUP));
    } else if (DirectoryService::PRIMARY_DS == mediator.m_ds->m_mode) {
      LOG_GENERAL(INFO, "DS leader node, upgrade after "
                            << TERMINATION_COUNTDOWN_IN_SECONDS +
                                   TERMINATION_COUNTDOWN_OFFSET_DS_LEADER
                            << " seconds...");
      this_thread::sleep_for(
          chrono::seconds(TERMINATION_COUNTDOWN_IN_SECONDS +
                          TERMINATION_COUNTDOWN_OFFSET_DS_LEADER));
    }
  }

  BlockStorage::GetBlockStorage().PutMetadata(MetaType::WAKEUPFORUPGRADE,
                                              {'1'});

  /// Deploy downloaded software
  if (LOOKUP_NODE_MODE) {
    boost::filesystem::copy_file(
        m_constantLookupFileName, CONSTANT_FILE_NAME,
        boost::filesystem::copy_option::overwrite_if_exists);
  } else if (ARCHIVAL_NODE) {
    boost::filesystem::copy_file(
        m_constantArchivalFileName, CONSTANT_FILE_NAME,
        boost::filesystem::copy_option::overwrite_if_exists);
  } else {
    boost::filesystem::copy_file(
        m_constantFileName, CONSTANT_FILE_NAME,
        boost::filesystem::copy_option::overwrite_if_exists);
  }

  /// TBD: The call of "dpkg" should be removed.
  /// (https://github.com/Zilliqa/Issues/issues/185)
  if (execl("/usr/bin/dpkg", "dpkg", "-i", m_packageFileName.data(), nullptr) <
      0) {
    LOG_GENERAL(WARNING, "Cannot deploy downloaded software!");
    return false;
  }

  /// Kill current node, then the recovery procedure will wake up node with
  /// stored data
  return raise(SIGKILL) == 0;
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
        initialDSCommittee.push_back(
            PubKey(DataConversion::HexStrToUint8Vec(ds_string), 0));
      }

      bytes message;

      unsigned int curr_offset = 0;
      for (auto& dsKey : initialDSCommittee) {
        dsKey.Serialize(message, curr_offset);
        curr_offset += PUB_KEY_SIZE;
      }

      string sig_str = ReadDSCommFile(signatureProp);
      string pubKey_str = ReadDSCommFile(publicKeyProp);

      PubKey pubKey(DataConversion::HexStrToUint8Vec(pubKey_str), 0);
      Signature sig(DataConversion::HexStrToUint8Vec(sig_str), 0);

      if (!Schnorr::GetInstance().Verify(message, sig, pubKey)) {
        LOG_GENERAL(WARNING, "Unable to verify file");
        return false;
      }
      return true;

    } else {
      vector<std::string> tempDsComm_string{ReadDSCommFromFile()};
      initialDSCommittee.clear();
      for (auto ds_string : tempDsComm_string) {
        initialDSCommittee.push_back(
            PubKey(DataConversion::HexStrToUint8Vec(ds_string), 0));
      }
    }

    return true;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, e.what());
    return false;
  }
}
