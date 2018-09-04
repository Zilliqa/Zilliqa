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

using namespace std;

#define RELEASE_URL                                                            \
    "https://api.github.com/repos/Zilliqa/Zilliqa/releases/latest"

SWInfo::SWInfo()
    : m_major(0)
    , m_minor(0)
    , m_fix(0)
    , m_upgradeDS(0)
    , m_commit(0)
{
}

SWInfo::SWInfo(const uint32_t& major, const uint32_t& minor,
               const uint32_t& fix, const uint64_t& upgradeDS,
               const uint32_t& commit)
    : m_major(major)
    , m_minor(minor)
    , m_fix(fix)
    , m_upgradeDS(upgradeDS)
    , m_commit(commit)
{
}

SWInfo::SWInfo(const SWInfo& src)
    : m_major(src.m_major)
    , m_minor(src.m_minor)
    , m_fix(src.m_fix)
    , m_upgradeDS(src.m_upgradeDS)
    , m_commit(src.m_commit)
{
}

SWInfo::~SWInfo(){};

/// Implements the Serialize function inherited from Serializable.
unsigned int SWInfo::Serialize(std::vector<unsigned char>& dst,
                               unsigned int offset) const
{
    LOG_MARKER();

    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < SIZE)
    {
        dst.resize(SIZE + offset);
    }

    unsigned int curOffset = offset;

    SetNumber<uint32_t>(dst, curOffset, m_major, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    SetNumber<uint32_t>(dst, curOffset, m_minor, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    SetNumber<uint32_t>(dst, curOffset, m_fix, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    SetNumber<uint64_t>(dst, curOffset, m_upgradeDS, sizeof(uint64_t));
    curOffset += sizeof(uint64_t);
    SetNumber<uint32_t>(dst, curOffset, m_commit, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);

    return SIZE;
}

/// Implements the Deserialize function inherited from Serializable.
int SWInfo::Deserialize(const std::vector<unsigned char>& src,
                        unsigned int offset)
{
    LOG_MARKER();

    unsigned int curOffset = offset;

    try
    {
        m_major = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
        curOffset += sizeof(uint32_t);
        m_minor = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
        curOffset += sizeof(uint32_t);
        m_fix = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
        curOffset += sizeof(uint32_t);
        m_upgradeDS = GetNumber<uint64_t>(src, curOffset, sizeof(uint64_t));
        curOffset += sizeof(uint64_t);
        m_commit = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
        curOffset += sizeof(uint32_t);
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with SWInfo::Deserialize." << ' ' << e.what());
        return -1;
    }

    return 0;
}

/// Less-than comparison operator.
bool SWInfo::operator<(const SWInfo& r) const
{
    return tie(m_major, m_minor, m_fix, m_upgradeDS, m_commit)
        < tie(r.m_major, r.m_minor, r.m_fix, r.m_upgradeDS, r.m_commit);
}

/// Greater-than comparison operator.
bool SWInfo::operator>(const SWInfo& r) const { return r < *this; }

/// Equality operator.
bool SWInfo::operator==(const SWInfo& r) const
{
    return tie(m_major, m_minor, m_fix, m_upgradeDS, m_commit)
        == tie(r.m_major, r.m_minor, r.m_fix, r.m_upgradeDS, r.m_commit);
}

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

    m_latestSWInfo = make_shared<SWInfo>(major, minor, fix, upgradeDS, commit);
    m_latestSHA = DataConversion::HexStrToUint8Vec(sha);
    return true;
}

bool UpgradeManager::ReplaceNode()
{
    LOG_MARKER();

    /// Store all the useful states into metadata, create a new node with loading the metadata, and kill current node
    /// TBD

    return true;
}
