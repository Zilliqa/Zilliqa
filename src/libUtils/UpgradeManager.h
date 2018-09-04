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

#include "libMediator/Mediator.h"
#include "libUtils/SWInfo.h"
#include <cstring>
#include <memory>
#include <string>

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
    bool ReplaceNode(Mediator& mediator);

    const std::shared_ptr<SWInfo> GetLatestSWInfo() { return m_latestSWInfo; }
};

#endif // __UPGRADEMANAGER_H__
