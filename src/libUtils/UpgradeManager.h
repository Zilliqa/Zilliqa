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

#ifndef __UPGRADEMANAGER_H__
#define __UPGRADEMANAGER_H__

#include "common/Serializable.h"
#include <cstring>
#include <memory>
#include <stdint.h>
#include <string>

class SWInfo : public Serializable
{
    uint32_t m_major;
    uint32_t m_minor;
    uint32_t m_fix;
    uint64_t m_upgradeDS;
    uint32_t m_commit;

public:
    static const unsigned int SIZE = sizeof(uint32_t) + sizeof(uint32_t)
        + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t);

    /// Default constructor for uninitialized version information.
    SWInfo();

    /// Constructor.
    SWInfo(const uint32_t& major, const uint32_t& minor, const uint32_t& fix,
           const uint64_t& upgradeDS, const uint32_t& commit);

    /// Destructor.
    ~SWInfo();

    /// Copy constructor.
    SWInfo(const SWInfo&);

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(std::vector<unsigned char>& dst,
                           unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    int Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

    /// Less-than comparison operator.
    bool operator<(const SWInfo& r) const;

    /// Greater-than comparison operator.
    bool operator>(const SWInfo& r) const;

    /// Equality operator.
    bool operator==(const SWInfo& r) const;
};

class UpgradeManager
{
private:
    std::shared_ptr<SWInfo> m_latestSWInfo;
    std::vector<unsigned char> m_latestSHA;

    UpgradeManager();
    ~UpgradeManager();

    // Singleton should not implement these
    UpgradeManager(UpgradeManager const&) = delete;
    void operator=(UpgradeManager const&) = delete;
    bool DownloadFile(const char* fileTail);

public:
    /// Returns the singleton UpgradeManager instance.
    static UpgradeManager& GetInstance();

    /// Check website, verify if sig is valid && SHA-256 is new
    bool HasNewSW();

    /// Download SW from website, then update current SHA-256 value & curSWInfo
    bool DownloadSW();

    /// Store all the useful states into metadata, create a new node with loading the metadata, and kill current node
    bool ReplaceNode();

    const std::shared_ptr<SWInfo> GetLatestSWInfo() { return m_latestSWInfo; }
};

#endif // __UPGRADEMANAGER_H__
