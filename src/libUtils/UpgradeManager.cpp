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
#include <experimental/filesystem>

using namespace std;

#define RELEASE_URL                                                            \
    "https://api.github.com/repos/Zilliqa/Zilliqa/releases/latest"

UpgradeManager::UpgradeManager() {}

UpgradeManager::~UpgradeManager() {}

UpgradeManager& UpgradeManager::GetInstance()
{
    static UpgradeManager um;
    return um;
}

bool UpgradeManager::DownloadFile(const char* fileTail)
{
    string cmd = string("curl -s ") + RELEASE_URL
        + " | grep \"browser_download_url.*" + fileTail
        + "\" | cut -d '\"' -f 4 | wget -qi -";
    return system(cmd.c_str()) >= 0;
}

bool UpgradeManager::HasNewSW()
{
    LOG_MARKER();

    if (!DownloadFile("pubKeyFile"))
    {
        LOG_GENERAL(WARNING, "Cannot download public key file!");
        return false;
    }

    if (!DownloadFile("VERSION"))
    {
        LOG_GENERAL(WARNING, "Cannot download version file!");
        return false;
    }

    vector<PubKey> pubKeys;
    {
        fstream pubKeyFile("pubKeyFile", ios::in);
        string pubKey;

        while (getline(pubKeyFile, pubKey))
        {
            pubKeys.emplace_back(DataConversion::HexStrToUint8Vec(pubKey), 0);
        }
    }

    string shaStr, sigStr;
    {
        fstream versionFile("VERSION", ios::in);
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

    if (!DownloadFile("VERSION"))
    {
        LOG_GENERAL(WARNING, "Cannot download version file!");
        return false;
    }

    if (!DownloadFile("deb"))
    {
        LOG_GENERAL(WARNING, "Cannot download package (.deb) file!");
        return false;
    }

    uint32_t major, minor, fix, commit;
    uint64_t upgradeDS;
    string sha;
    {
        fstream versionFile("VERSION", ios::in);
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

    /// Verify SHA-256 checksum of .deb file
    experimental::filesystem::recursive_directory_iterator it("./"), endit;
    string debFileName;

    while (it != endit)
    {
        if (experimental::filesystem::is_regular_file(*it)
            && it->path().extension() == ".deb")
        {
            debFileName = it->path().filename();
            break;
        }

        ++it;
    }

    if (debFileName.empty())
    {
        LOG_GENERAL(WARNING, "Cannot find package (.deb) file!");
        return false;
    }

    string downloadSha;
    {
        fstream debFile(debFileName, ios::in);

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

    /// Deploy downloaded software
    if (system("dpkg -i *.deb") < 0)
    {
        LOG_GENERAL(WARNING, "Cannot deploy downloaded software!");
        return false;
    }

    /// Kill current node, then the recovery procedure will wake up node with stored data
    return raise(SIGKILL) == 0;
}
