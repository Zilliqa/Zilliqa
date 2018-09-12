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

#include "libUtils/Logger.h"
#include "libUtils/UpgradeManager.h"

#define BOOST_TEST_MODULE upgradeManager
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#define TEST_RELEASE_URL                                                       \
    "https://api.github.com/repos/ckyang/Zilliqa/releases/latest"

using namespace std;

BOOST_AUTO_TEST_SUITE(upgradeManager)

BOOST_AUTO_TEST_CASE(test_downloadFile_Integrity)
{
    INIT_STDOUT_LOGGER();
    LOG_GENERAL(INFO, "Running test_downloadFile_Integrity...");

    string expectedFileName = "NotExistedFile";
    remove(expectedFileName.data());
    BOOST_CHECK_MESSAGE(!ifstream(expectedFileName.data()).good(),
                        "File still exists, cannot test!");
    string fileName = UpgradeManager::GetInstance().DownloadFile(
        "NotExistedFile", TEST_RELEASE_URL);
    BOOST_CHECK_MESSAGE(!ifstream(expectedFileName.data()).good(),
                        "Some abnormal file downloaded!");
    BOOST_CHECK_MESSAGE(fileName.empty(), "Some abnormal file downloaded!");

    LOG_GENERAL(INFO, "Verify test_downloadFile_Integrity is successfully.");
}

BOOST_AUTO_TEST_CASE(test_downloadFile_VERSION)
{
    INIT_STDOUT_LOGGER();
    LOG_GENERAL(INFO, "Running test_downloadFile_VERSION...");

    string expectedFileName = "VERSION";
    remove(expectedFileName.data());
    BOOST_CHECK_MESSAGE(!ifstream(expectedFileName.data()).good(),
                        "File still exists, cannot test!");
    string fileName = UpgradeManager::GetInstance().DownloadFile(
        "VERSION", TEST_RELEASE_URL);
    BOOST_CHECK_MESSAGE(ifstream(expectedFileName.data()).good(),
                        "File not downloaded!");
    BOOST_CHECK_MESSAGE(fileName == expectedFileName, "Download wrong file!");

    LOG_GENERAL(INFO, "Verify test_downloadFile_VERSION is successfully.");
}

BOOST_AUTO_TEST_CASE(test_downloadFile_pubKeyFile)
{
    INIT_STDOUT_LOGGER();
    LOG_GENERAL(INFO, "Running test_downloadFile_pubKeyFile...");

    string expectedFileName = "pubKeyFile";
    remove(expectedFileName.data());
    BOOST_CHECK_MESSAGE(!ifstream(expectedFileName.data()).good(),
                        "File still exists, cannot test!");
    string fileName = UpgradeManager::GetInstance().DownloadFile(
        "pubKeyFile", TEST_RELEASE_URL);
    BOOST_CHECK_MESSAGE(ifstream(expectedFileName.data()).good(),
                        "File not downloaded!");
    BOOST_CHECK_MESSAGE(fileName == expectedFileName, "Download wrong file!");

    LOG_GENERAL(INFO, "Verify test_downloadFile_pubKeyFile is successfully.");
}

BOOST_AUTO_TEST_CASE(test_downloadFile_deb)
{
    INIT_STDOUT_LOGGER();
    LOG_GENERAL(INFO, "Running test_downloadFile_deb...");

    string expectedFileName = "D24-1.0.0.5.a9a4c93-Linux.deb";
    remove(expectedFileName.data());
    BOOST_CHECK_MESSAGE(!ifstream(expectedFileName.data()).good(),
                        "File still exists, cannot test!");
    string fileName
        = UpgradeManager::GetInstance().DownloadFile("deb", TEST_RELEASE_URL);
    BOOST_CHECK_MESSAGE(ifstream(expectedFileName.data()).good(),
                        "File not downloaded!");
    BOOST_CHECK_MESSAGE(fileName == expectedFileName, "Download wrong file!");

    LOG_GENERAL(INFO, "Verify test_downloadFile_deb is successfully.");
}

BOOST_AUTO_TEST_SUITE_END()
