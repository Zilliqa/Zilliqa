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
#define VERSION_FILE_NAME "VERSION"
#define PUBLIC_KEY_FILE_NAME "pubKeyFile"
#define CONSTANT_FILE_NAME "constants.xml"
#define CONSTANT_LOOKUP_FILE_NAME "constants.xml_lookup"
#define CONSTANT_SEED_FILE_NAME "constants.xml_archivallookup"
#define ZILLIQA_PACKAGE_FILE_EXTENSION "-Zilliqa.deb"
#define SCILLA_PACKAGE_FILE_EXTENSION "-Scilla.deb"
#define DPKG_BINARY_PATH "/usr/bin/dpkg"
#define DPKG_CONFIG_PATH "/var/lib/dpkg/status"
#define UPGRADE_HOST                                                      \
  string(string("https://api.github.com/repos/") + UPGRADE_HOST_ACCOUNT + \
         "/" + UPGRADE_HOST_REPO + "/releases/latest")

const unsigned int TERMINATION_COUNTDOWN_OFFSET_SHARD = 0;
const unsigned int TERMINATION_COUNTDOWN_OFFSET_DS_BACKUP = 1;
const unsigned int TERMINATION_COUNTDOWN_OFFSET_DS_LEADER = 2;
const unsigned int TERMINATION_COUNTDOWN_OFFSET_LOOKUP = 3;

enum VERSION_LINE : unsigned int {
  ZILLIQA_MAJOR_VERSION_LINE = 2,
  ZILLIQA_MINOR_VERSION_LINE = 4,
  ZILLIQA_FIX_VERSION_LINE = 6,
  ZILLIQA_DS_LINE = 8,
  SCILLA_DS_LINE = 10,
  SCILLA_MAJOR_VERSION_LINE = 14,
  SCILLA_MINOR_VERSION_LINE = 16,
  SCILLA_FIX_VERSION_LINE = 18,
  ZILLIQA_COMMIT_LINE = 20,
  ZILLIQA_SHA_LINE = 22,
  ZILLIQA_SIG_LINE = 24,
  SCILLA_COMMIT_LINE = 26,
  SCILLA_SHA_LINE = 28,
  SCILLA_SIG_LINE = 30,
};

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

  try {
    string line;
    fstream pubFile(pubKeyFileName, ios::in);

    while (getline(pubFile, line)) {
      try {
        pubKeys.push_back(PubKey::GetPubKeyFromString(line));
      } catch (std::invalid_argument& e) {
        std::cerr << e.what() << endl;
      }
    }
  } catch (std::exception& e) {
    std::cerr << "Problem occured when processing public keys on line: "
              << pubKeys.size() + 1 << endl;
  }

  LOG_GENERAL(INFO, "Public key file has been parsed successfully.");
  shared_ptr<PubKey> aggregatedPubkey = MultiSig::AggregatePubKeys(pubKeys);

  string zilliqaShaStr, zilliqaSigStr, scillaShaStr, scillaSigStr;
  {
    fstream versionFile(versionName, ios::in);
    int line_no = 0;

    while (line_no != ZILLIQA_SHA_LINE && getline(versionFile, zilliqaShaStr)) {
      ++line_no;
    }

    while (line_no != ZILLIQA_SIG_LINE && getline(versionFile, zilliqaSigStr)) {
      ++line_no;
    }

    while (line_no != SCILLA_SHA_LINE && getline(versionFile, scillaShaStr)) {
      ++line_no;
    }

    while (line_no != SCILLA_SIG_LINE && getline(versionFile, scillaSigStr)) {
      ++line_no;
    }
  }

  LOG_GENERAL(INFO, "Version file has been parsed successfully.");

  if (zilliqaSigStr != "0") {
    const bytes zilliqaSha(zilliqaShaStr.begin(), zilliqaShaStr.end());
    bytes tempMultisigBytes;
    DataConversion::HexStrToUint8Vec(zilliqaSigStr, tempMultisigBytes);
    shared_ptr<Signature> zilliqaSig(new Signature(tempMultisigBytes, 0));

    /// Multi-sig verification
    if (!MultiSig::GetInstance().MultiSigVerify(zilliqaSha, *zilliqaSig,
                                                *aggregatedPubkey)) {
      LOG_GENERAL(WARNING, "Multisig verification on Zilliqa failed!");
      return false;
    }

    if (m_latestZilliqaSHA != zilliqaSha) {
      return true;
    }
  }

  if (scillaSigStr != "0") {
    const bytes scillaSha(scillaShaStr.begin(), scillaShaStr.end());
    bytes tempMultisigBytes;
    DataConversion::HexStrToUint8Vec(scillaSigStr, tempMultisigBytes);
    shared_ptr<Signature> scillaSig(new Signature(tempMultisigBytes, 0));

    /// Multi-sig verification
    if (!MultiSig::GetInstance().MultiSigVerify(scillaSha, *scillaSig,
                                                *aggregatedPubkey)) {
      LOG_GENERAL(WARNING, "Multisig verification on Scilla failed!");
      return false;
    }

    if (m_latestScillaSHA != scillaSha) {
      return true;
    }
  }

  return false;
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

  m_constantArchivalLookupFileName = DownloadFile(CONSTANT_SEED_FILE_NAME);

  if (m_constantArchivalLookupFileName.empty()) {
    LOG_GENERAL(WARNING, "Cannot download constant archival lookup seed file!");
  }

  LOG_GENERAL(INFO, "Constant file has been downloaded successfully.");

  m_zilliqaPackageFileName = DownloadFile(ZILLIQA_PACKAGE_FILE_EXTENSION);

  if (m_zilliqaPackageFileName.empty()) {
    LOG_GENERAL(INFO, "Cannot download Zilliqa package (.deb) file!");
  }

  m_scillaPackageFileName = DownloadFile(SCILLA_PACKAGE_FILE_EXTENSION);

  if (m_scillaPackageFileName.empty()) {
    LOG_GENERAL(INFO, "Cannot download Scilla package (.deb) file!");
  }

  if (m_zilliqaPackageFileName.empty() && m_scillaPackageFileName.empty()) {
    LOG_GENERAL(WARNING, "No package downloaded, nothing will be upgraded!");
    m_latestSWInfo = make_shared<SWInfo>();
    DataConversion::HexStrToUint8Vec("0", m_latestZilliqaSHA);
    DataConversion::HexStrToUint8Vec("0", m_latestScillaSHA);
    return false;
  }

  LOG_GENERAL(INFO, "Package (.deb) file has been downloaded successfully.");

  uint32_t zilliqaMajor, zilliqaMinor, zilliqaFix, zilliqaCommit, scillaMajor,
      scillaMinor, scillaFix, scillaCommit;
  uint64_t zilliqaUpgradeDS, scillaUpgradeDS;
  string zilliqaSha, scillaSha;

  try {
    fstream versionFile(versionName, ios::in);
    int line_no = 0;
    string line;

    while (line_no != ZILLIQA_MAJOR_VERSION_LINE &&
           getline(versionFile, line)) {
      ++line_no;
    }

    zilliqaMajor = stoul(line);

    while (line_no != ZILLIQA_MINOR_VERSION_LINE &&
           getline(versionFile, line)) {
      ++line_no;
    }

    zilliqaMinor = stoul(line);

    while (line_no != ZILLIQA_FIX_VERSION_LINE && getline(versionFile, line)) {
      ++line_no;
    }

    zilliqaFix = stoul(line);

    while (line_no != ZILLIQA_DS_LINE && getline(versionFile, line)) {
      ++line_no;
    }

    zilliqaUpgradeDS = m_zilliqaPackageFileName.empty() ? 0 : stoull(line);

    while (line_no != SCILLA_DS_LINE && getline(versionFile, line)) {
      ++line_no;
    }

    scillaUpgradeDS = m_scillaPackageFileName.empty() ? 0 : stoull(line);

    while (line_no != SCILLA_MAJOR_VERSION_LINE && getline(versionFile, line)) {
      ++line_no;
    }

    scillaMajor = stoul(line);

    while (line_no != SCILLA_MINOR_VERSION_LINE && getline(versionFile, line)) {
      ++line_no;
    }

    scillaMinor = stoul(line);

    while (line_no != SCILLA_FIX_VERSION_LINE && getline(versionFile, line)) {
      ++line_no;
    }

    scillaFix = stoul(line);

    while (line_no != ZILLIQA_COMMIT_LINE && getline(versionFile, line)) {
      ++line_no;
    }

    zilliqaCommit = stoul(line, nullptr, 16);

    while (line_no != ZILLIQA_SHA_LINE && getline(versionFile, zilliqaSha)) {
      ++line_no;
    }

    while (line_no != SCILLA_COMMIT_LINE && getline(versionFile, line)) {
      ++line_no;
    }

    scillaCommit = stoul(line, nullptr, 16);

    while (line_no != SCILLA_SHA_LINE && getline(versionFile, scillaSha)) {
      ++line_no;
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Cannot parse " << VERSION_FILE_NAME << ": " << ' '
                                         << e.what());
    return false;
  }

  /// Verify SHA-256 checksum of .deb file
  if (!m_zilliqaPackageFileName.empty()) {
    fstream debFile(m_zilliqaPackageFileName, ios::in);
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    bytes vec((istreambuf_iterator<char>(debFile)),
              (istreambuf_iterator<char>()));
    sha2.Update(vec, 0, vec.size());
    bytes output = sha2.Finalize();
    string zilliqaDownloadSha;
    DataConversion::Uint8VecToHexStr(output, zilliqaDownloadSha);

    if (zilliqaSha != zilliqaDownloadSha) {
      LOG_GENERAL(WARNING,
                  "Zilliqa SHA-256 checksum of .deb file mismatch. Expected: "
                      << zilliqaSha << " Actual: " << zilliqaDownloadSha);
      return false;
    }

    DataConversion::HexStrToUint8Vec(zilliqaSha, m_latestZilliqaSHA);
  }

  if (!m_scillaPackageFileName.empty()) {
    fstream debFile(m_scillaPackageFileName, ios::in);
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    bytes vec((istreambuf_iterator<char>(debFile)),
              (istreambuf_iterator<char>()));
    sha2.Update(vec, 0, vec.size());
    bytes output = sha2.Finalize();
    string scillaDownloadSha;
    DataConversion::Uint8VecToHexStr(output, scillaDownloadSha);

    if (scillaSha != scillaDownloadSha) {
      LOG_GENERAL(WARNING,
                  "Scilla SHA-256 checksum of .deb file mismatch. Expected: "
                      << scillaSha << " Actual: " << scillaDownloadSha);
      return false;
    }

    DataConversion::HexStrToUint8Vec(scillaSha, m_latestScillaSHA);
  }

  m_latestSWInfo = make_shared<SWInfo>(
      zilliqaMajor, zilliqaMinor, zilliqaFix, zilliqaUpgradeDS, zilliqaCommit,
      scillaMajor, scillaMinor, scillaFix, scillaUpgradeDS, scillaCommit);
  return true;
}

bool UpgradeManager::ReplaceNode(Mediator& mediator) {
  LOG_MARKER();

  auto func = [this, &mediator]() mutable -> void {
    if (LOOKUP_NODE_MODE) {
      LOG_GENERAL(INFO, "Lookup node, will be upgraded after "
                            << TERMINATION_COUNTDOWN_IN_SECONDS +
                                   TERMINATION_COUNTDOWN_OFFSET_LOOKUP
                            << " seconds...");
      this_thread::sleep_for(
          chrono::seconds(TERMINATION_COUNTDOWN_IN_SECONDS +
                          TERMINATION_COUNTDOWN_OFFSET_LOOKUP));

      BlockStorage::GetBlockStorage().PutMetadata(MetaType::DSINCOMPLETED,
                                                  {'0'});
    } else {
      if (DirectoryService::IDLE == mediator.m_ds->m_mode) {
        LOG_GENERAL(INFO, "Shard node, will be upgraded after "
                              << TERMINATION_COUNTDOWN_IN_SECONDS +
                                     TERMINATION_COUNTDOWN_OFFSET_SHARD
                              << " seconds...");
        this_thread::sleep_for(
            chrono::seconds(TERMINATION_COUNTDOWN_IN_SECONDS +
                            TERMINATION_COUNTDOWN_OFFSET_SHARD));
      } else if (DirectoryService::BACKUP_DS == mediator.m_ds->m_mode) {
        LOG_GENERAL(INFO, "DS backup node, will be upgraded after "
                              << TERMINATION_COUNTDOWN_IN_SECONDS +
                                     TERMINATION_COUNTDOWN_OFFSET_DS_BACKUP
                              << " seconds...");
        this_thread::sleep_for(
            chrono::seconds(TERMINATION_COUNTDOWN_IN_SECONDS +
                            TERMINATION_COUNTDOWN_OFFSET_DS_BACKUP));
      } else if (DirectoryService::PRIMARY_DS == mediator.m_ds->m_mode) {
        LOG_GENERAL(INFO, "DS leader node, will be upgraded after "
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
    if (ARCHIVAL_LOOKUP) {
      boost::filesystem::copy_file(
          m_constantArchivalLookupFileName, CONSTANT_FILE_NAME,
          boost::filesystem::copy_option::overwrite_if_exists);
    } else if (LOOKUP_NODE_MODE) {
      boost::filesystem::copy_file(
          m_constantLookupFileName, CONSTANT_FILE_NAME,
          boost::filesystem::copy_option::overwrite_if_exists);
    } else {
      boost::filesystem::copy_file(
          m_constantFileName, CONSTANT_FILE_NAME,
          boost::filesystem::copy_option::overwrite_if_exists);
    }

    /// TBD: The call of "dpkg" should be removed.
    /// (https://github.com/Zilliqa/Issues/issues/185)
    if (execl(DPKG_BINARY_PATH, "dpkg", "-i", m_zilliqaPackageFileName.data(),
              nullptr) < 0) {
      LOG_GENERAL(WARNING, "Cannot deploy downloaded Zilliqa software!");
      return;
    }

    /// Kill current node, then the recovery procedure will wake up node with
    /// stored data
    raise(SIGKILL);
  };
  DetachedFunction(1, func);
  return true;
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

bool UpgradeManager::InstallScilla() {
  LOG_MARKER();

  auto func = [this]() mutable -> void {
    if (!m_scillaPackageFileName.empty()) {
      if (!UpgradeManager::UnconfigureScillaPackage()) {
        return;
      }

      LOG_GENERAL(INFO, "Installing Scilla...");

      pid_t pid = fork();

      if (pid == -1) {
        LOG_GENERAL(WARNING, "Cannot fork a process for installing scilla!");
        return;
      }

      if (pid > 0) {
        /// Parent process
        int status;
        do {
          if ((pid = waitpid(pid, &status, WNOHANG)) == -1) {
            perror("wait() error");
          } else if (pid == 0) {
            LOG_GENERAL(INFO, "Keeping installing scilla...");
            this_thread::sleep_for(chrono::seconds(1));
          } else {
            if (WIFEXITED(status)) {
              LOG_GENERAL(INFO, "Scilla has been installed successfully.");
            } else {
              LOG_GENERAL(WARNING, "Failed to install scilla with status "
                                       << WEXITSTATUS(status));
              return;
            }
          }
        } while (pid == 0);
      } else {
        /// Child process
        if (execl(DPKG_BINARY_PATH, "dpkg", "-i",
                  m_scillaPackageFileName.data(), nullptr) < 0) {
          LOG_GENERAL(WARNING, "Cannot deploy downloaded Scilla software!");
        }

        exit(0);
      }
    }
  };
  DetachedFunction(1, func);
  return true;
}

bool UpgradeManager::UnconfigureScillaPackage() {
  LOG_MARKER();
  const string dpkgStatusFileName(DPKG_CONFIG_PATH), tmpFileName("temp.txt");
  ifstream dpkgStatusFile;
  dpkgStatusFile.open(dpkgStatusFileName);
  ofstream tempFile;
  tempFile.open(tmpFileName);
  string line;

  while (getline(dpkgStatusFile, line)) {
    if (line.find("scilla") != string::npos) {
      getline(dpkgStatusFile, line);
      getline(dpkgStatusFile, line);
      getline(dpkgStatusFile, line);
      getline(dpkgStatusFile, line);
      getline(dpkgStatusFile, line);
      continue;
    }

    tempFile << line << endl;
  }

  tempFile.close();
  dpkgStatusFile.close();
  boost::filesystem::copy_file(
      tmpFileName, dpkgStatusFileName,
      boost::filesystem::copy_option::overwrite_if_exists);
  remove(tmpFileName.c_str());

  return true;
}
