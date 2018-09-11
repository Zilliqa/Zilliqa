/**
* Copyright (c) 2018 Zilliqa 
* This source code is being disclosed to you solely for the purpose of your participation in 
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
* the protocols and algorithms that are programmed into, and intended by, the code. You may 
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
* including modifying or publishing the code (or any part of it), and developing or forming 
* another public or private blockchain network. This source code is provided ‘as is’ and no 
* warranties are given as to title or non-infringement, merchantability or fitness for purpose 
* and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
* Some programs in this code are governed by the GNU General Public License v3.0 (available at 
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
* and which include a reference to GPLv3 in their program files.
**/

#include "UpgradeManager.h"
#include "libCrypto/Schnorr.h"
#include "libUtils/Logger.h"
#include <boost/tokenizer.hpp>
using namespace std;

#define RELEASE_URL                                                            \
    "https://api.github.com/repos/Zilliqa/Zilliqa/releases/latest"
#define USER_AGENT "Zilliqa"
#define VERSION_FILE_NAME "VERSION"
#define PUBLIC_KEY_FILE_NAME "pubKeyFile"
#define PACKAGE_FILE_EXTENSION "deb"

UpgradeManager::UpgradeManager()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    m_curl = curl_easy_init();

    if (!m_curl)
    {
        LOG_GENERAL(WARNING, "curl initialization fail!");
        curl_global_cleanup();
    }
}

UpgradeManager::~UpgradeManager()
{
    if (m_curl)
    {
        curl_easy_cleanup(m_curl);
        curl_global_cleanup();
        m_curl = nullptr;
    }
}

UpgradeManager& UpgradeManager::GetInstance()
{
    static UpgradeManager um;
    return um;
}

static size_t WriteString(void* contents, size_t size, size_t nmemb,
                          void* userp)
{
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string UpgradeManager::DownloadFile(const char* fileTail)
{
    if (!m_curl)
    {
        LOG_GENERAL(WARNING, "Cannot perform any curl operation!");
        return "";
    }

    string curlRes;
    curl_easy_reset(m_curl);
    curl_easy_setopt(m_curl, CURLOPT_URL, RELEASE_URL);
    curl_easy_setopt(m_curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, WriteString);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &curlRes);
    CURLcode res = curl_easy_perform(m_curl);

    if (res != CURLE_OK)
    {
        LOG_GENERAL(WARNING,
                    "curl_easy_perform() failed: " << curl_easy_strerror(res));
        return "";
    }

    int find = 0;
    string cur;
    vector<string> downloadFilePaths;
    boost::char_separator<char> sep(",\"");
    boost::tokenizer<boost::char_separator<char>> tokens(curlRes, sep);
    for (boost::tokenizer<boost::char_separator<char>>::iterator tok_iter
         = tokens.begin();
         tok_iter != tokens.end(); ++tok_iter)
    {
        if (1 == find)
        {
            ++find;
            continue;
        }

        if (2 == find)
        {
            find = 0;
            downloadFilePaths.emplace_back(*tok_iter);
            continue;
        }

        cur = *tok_iter;

        if (cur == "browser_download_url")
        {
            find = 1;
            continue;
        }
    }

    string downloadFilePath;

    for (auto s : downloadFilePaths)
    {
        if (string::npos != s.rfind(fileTail))
        {
            downloadFilePath = s;
            break;
        }
    }

    string fileName = downloadFilePath.substr(downloadFilePath.rfind('/') + 1);

    /// Get the redirection url (if applicable)
    curl_easy_reset(m_curl);
    curl_easy_setopt(m_curl, CURLOPT_URL, downloadFilePath.data());
    res = curl_easy_perform(m_curl);

    if (res != CURLE_OK)
    {
        LOG_GENERAL(WARNING,
                    "curl_easy_perform() failed: " << curl_easy_strerror(res));
        return "";
    }

    long response_code;
    res = curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &response_code);

    if ((res != CURLE_OK) || ((response_code / 100) == 3))
    {
        char* location;
        curl_easy_getinfo(m_curl, CURLINFO_REDIRECT_URL, &location);

        if ((res == CURLE_OK) && location)
        {
            downloadFilePath = location;
        }
    }

    /// Download the file
    curl_easy_reset(m_curl);
    curl_easy_setopt(m_curl, CURLOPT_URL, downloadFilePath.data());
    curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 1L);
    FILE* file = fopen(fileName.data(), "wb");

    if (!file)
    {
        return "";
    }

    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, file);
    res = curl_easy_perform(m_curl);

    if (res != CURLE_OK)
    {
        LOG_GENERAL(WARNING,
                    "curl_easy_perform() failed: " << curl_easy_strerror(res));
        fclose(file);
        return "";
    }

    fclose(file);
    return fileName;
}

bool UpgradeManager::HasNewSW()
{
    LOG_MARKER();

    string pubKeyFileName = DownloadFile(PUBLIC_KEY_FILE_NAME);

    if (pubKeyFileName.empty())
    {
        LOG_GENERAL(WARNING, "Cannot download public key file!");
        return false;
    }

    string versionName = DownloadFile(VERSION_FILE_NAME);

    if (versionName.empty())
    {
        LOG_GENERAL(WARNING, "Cannot download version file!");
        return false;
    }

    vector<PubKey> pubKeys;
    {
        fstream pubKeyFile(PUBLIC_KEY_FILE_NAME, ios::in);
        string pubKey;

        while (getline(pubKeyFile, pubKey))
        {
            pubKeys.emplace_back(DataConversion::HexStrToUint8Vec(pubKey), 0);
        }
    }

    string shaStr, sigStr;
    {
        fstream versionFile(VERSION_FILE_NAME, ios::in);
        int line_no = 0;

        /// Read SHA-256 hash
        while (line_no != 14 && getline(versionFile, shaStr))
        {
            ++line_no;
        }

        /// Read signature
        while (line_no != 16 && getline(versionFile, sigStr))
        {
            ++line_no;
        }
    }

    const vector<unsigned char> sha = DataConversion::HexStrToUint8Vec(shaStr);
    const unsigned int len = sigStr.size() / pubKeys.size();
    vector<Signature> mutliSig;

    for (unsigned int i = 0; i < pubKeys.size(); ++i)
    {
        mutliSig.emplace_back(
            DataConversion::HexStrToUint8Vec(sigStr.substr(i * len, len)), 0);
    }

    /// Multi-sig verification
    for (unsigned int i = 0; i < pubKeys.size(); ++i)
    {
        if (!Schnorr::GetInstance().Verify(sha, mutliSig.at(i), pubKeys.at(i)))
        {
            LOG_GENERAL(WARNING, "Multisig verification failed!");
            return false;
        }
    }

    return m_latestSHA != sha;
}

bool UpgradeManager::DownloadSW()
{
    LOG_MARKER();

    string versionName = DownloadFile(VERSION_FILE_NAME);

    if (versionName.empty())
    {
        LOG_GENERAL(WARNING, "Cannot download version file!");
        return false;
    }

    m_packageFileName = DownloadFile(PACKAGE_FILE_EXTENSION);

    if (m_packageFileName.empty())
    {
        LOG_GENERAL(WARNING, "Cannot download package (.deb) file!");
        return false;
    }

    uint32_t major, minor, fix, commit;
    uint64_t upgradeDS;
    string sha;

    try
    {
        fstream versionFile(VERSION_FILE_NAME, ios::in);
        int line_no = 0;
        string line;

        /// Read major version
        while (line_no != 2 && getline(versionFile, line))
        {
            ++line_no;
        }

        major = stoul(line);

        /// Read minor version
        while (line_no != 4 && getline(versionFile, line))
        {
            ++line_no;
        }

        minor = stoul(line);

        /// Read fix version
        while (line_no != 6 && getline(versionFile, line))
        {
            ++line_no;
        }

        fix = stoul(line);

        /// Read expected DS epoch
        while (line_no != 8 && getline(versionFile, line))
        {
            ++line_no;
        }

        upgradeDS = stoull(line);

        /// Read Git commit ID
        while (line_no != 12 && getline(versionFile, line))
        {
            ++line_no;
        }

        commit = stoul(line, nullptr, 16);

        /// Read SHA-256 hash
        while (line_no != 14 && getline(versionFile, sha))
        {
            ++line_no;
        }
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Cannot parse " << VERSION_FILE_NAME << ": " << ' '
                                    << e.what());
        return false;
    }

    /// Verify SHA-256 checksum of .deb file
    string downloadSha;
    {
        fstream debFile(m_packageFileName, ios::in);

        SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
        vector<unsigned char> vec((istreambuf_iterator<char>(debFile)),
                                  (istreambuf_iterator<char>()));
        sha2.Update(vec, 0, vec.size());
        vector<unsigned char> output = sha2.Finalize();
        downloadSha = DataConversion::Uint8VecToHexStr(output);
    }

    if (sha != downloadSha)
    {
        LOG_GENERAL(WARNING, "SHA-256 checksum of .deb file does not match!");
        return false;
    }

    m_latestSWInfo = make_shared<SWInfo>(major, minor, fix, upgradeDS, commit);
    m_latestSHA = DataConversion::HexStrToUint8Vec(sha);
    return true;
}

bool UpgradeManager::ReplaceNode(Mediator& mediator)
{
    LOG_MARKER();

    /// Store states
    AccountStore::GetInstance().UpdateStateTrieAll();
    AccountStore::GetInstance().MoveUpdatesToDisk();

    /// Store DS block
    vector<unsigned char> serializedDSBlock;
    mediator.m_dsBlockChain.GetLastBlock().Serialize(serializedDSBlock, 0);
    BlockStorage::GetBlockStorage().PutDSBlock(
        mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum(),
        serializedDSBlock);

    /// Store final block
    vector<unsigned char> serializedTxBlock;
    mediator.m_txBlockChain.GetLastBlock().Serialize(serializedTxBlock, 0);
    BlockStorage::GetBlockStorage().PutTxBlock(
        mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum(),
        serializedTxBlock);

    /// TBD: The system call of "dpkg" should be removed. (https://github.com/Zilliqa/Issues/issues/185)
    string cmd = string("dpkg -i ") + m_packageFileName;

    /// Deploy downloaded software
    if (system(cmd.data()) < 0)
    {
        LOG_GENERAL(WARNING, "Cannot deploy downloaded software!");
        return false;
    }

    /// Kill current node, then the recovery procedure will wake up node with stored data
    return raise(SIGKILL) == 0;
}
