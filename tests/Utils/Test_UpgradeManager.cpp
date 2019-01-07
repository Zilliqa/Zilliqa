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

#include "libUtils/Logger.h"
#include "libUtils/UpgradeManager.h"

#define BOOST_TEST_MODULE upgradeManager
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(upgradeManager)

BOOST_AUTO_TEST_CASE(test_curl) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Running test_curl");

  CURL *curl = curl_easy_init();
  struct curl_slist *headers = NULL;
  string token = "";
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(
      curl, CURLOPT_URL,
      "https://api.github.com/repos/ckyang/Zilliqa/releases/latest");
  string token_header = "Authorization: token " + token;
  headers = curl_slist_append(headers, token_header.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "Zilliqa");
  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    LOG_GENERAL(WARNING,
                "curl_easy_perform() failed to get latest release "
                "information"
                    << curl_easy_strerror(res));
  }

  curl_slist_free_all(headers);

  LOG_GENERAL(INFO, "Verify test_curl completed.");
}

BOOST_AUTO_TEST_CASE(test_downloadFile_Integrity) {
  INIT_STDOUT_LOGGER();

  if (2 != boost::unit_test::framework::master_test_suite().argc) {
    LOG_GENERAL(WARNING,
                "Please provide test repo name! ex: ./Test_UpgradeManager "
                "https://api.github.com/repos/ckyang/Zilliqa/"
                "releases/latest");
    return;
  }

  LOG_GENERAL(INFO, "Running test_downloadFile_Integrity...");

  string expectedFileName = "NotExistedFile";
  remove(expectedFileName.data());
  BOOST_CHECK_MESSAGE(!ifstream(expectedFileName.data()).good(),
                      "File still exists, cannot test!");
  string fileName = UpgradeManager::GetInstance().DownloadFile(
      "NotExistedFile",
      boost::unit_test::framework::master_test_suite().argv[1]);
  BOOST_CHECK_MESSAGE(!ifstream(expectedFileName.data()).good(),
                      "Some abnormal file downloaded!");
  BOOST_CHECK_MESSAGE(fileName.empty(), "Some abnormal file downloaded!");

  LOG_GENERAL(INFO, "Verify test_downloadFile_Integrity completed.");
}

BOOST_AUTO_TEST_CASE(test_downloadFile_VERSION) {
  INIT_STDOUT_LOGGER();

  if (2 != boost::unit_test::framework::master_test_suite().argc) {
    LOG_GENERAL(WARNING,
                "Please provide test repo name! ex: ./Test_UpgradeManager "
                "https://api.github.com/repos/ckyang/Zilliqa/"
                "releases/latest");
    return;
  }

  LOG_GENERAL(INFO, "Running test_downloadFile_VERSION...");

  string expectedFileName = "VERSION";
  remove(expectedFileName.data());
  BOOST_CHECK_MESSAGE(!ifstream(expectedFileName.data()).good(),
                      "File still exists, cannot test!");
  string fileName = UpgradeManager::GetInstance().DownloadFile(
      "VERSION", boost::unit_test::framework::master_test_suite().argv[1]);
  BOOST_CHECK_MESSAGE(ifstream(expectedFileName.data()).good(),
                      "File not downloaded!");
  BOOST_CHECK_MESSAGE(fileName == expectedFileName, "Download wrong file!");

  LOG_GENERAL(INFO, "Verify test_downloadFile_VERSION completed.");
}

BOOST_AUTO_TEST_CASE(test_downloadFile_pubKeyFile) {
  INIT_STDOUT_LOGGER();

  if (2 != boost::unit_test::framework::master_test_suite().argc) {
    LOG_GENERAL(WARNING,
                "Please provide test repo name! ex: ./Test_UpgradeManager "
                "https://api.github.com/repos/ckyang/Zilliqa/"
                "releases/latest");
    return;
  }

  LOG_GENERAL(INFO, "Running test_downloadFile_pubKeyFile...");

  string expectedFileName = "pubKeyFile";
  remove(expectedFileName.data());
  BOOST_CHECK_MESSAGE(!ifstream(expectedFileName.data()).good(),
                      "File still exists, cannot test!");
  string fileName = UpgradeManager::GetInstance().DownloadFile(
      "pubKeyFile", boost::unit_test::framework::master_test_suite().argv[1]);
  BOOST_CHECK_MESSAGE(ifstream(expectedFileName.data()).good(),
                      "File not downloaded!");
  BOOST_CHECK_MESSAGE(fileName == expectedFileName, "Download wrong file!");

  LOG_GENERAL(INFO, "Verify test_downloadFile_pubKeyFile completed.");
}

BOOST_AUTO_TEST_CASE(test_downloadFile_deb) {
  INIT_STDOUT_LOGGER();

  if (2 != boost::unit_test::framework::master_test_suite().argc) {
    LOG_GENERAL(WARNING,
                "Please provide test repo name! ex: ./Test_UpgradeManager "
                "https://api.github.com/repos/ckyang/Zilliqa/"
                "releases/latest");
    return;
  }

  LOG_GENERAL(INFO, "Running test_downloadFile_deb...");

  string expectedFileName = "D24-1.0.0.5.a9a4c93-Linux.deb";
  remove(expectedFileName.data());
  BOOST_CHECK_MESSAGE(!ifstream(expectedFileName.data()).good(),
                      "File still exists, cannot test!");
  string fileName = UpgradeManager::GetInstance().DownloadFile(
      "deb", boost::unit_test::framework::master_test_suite().argv[1]);
  BOOST_CHECK_MESSAGE(ifstream(expectedFileName.data()).good(),
                      "File not downloaded!");
  BOOST_CHECK_MESSAGE(fileName == expectedFileName, "Download wrong file!");

  LOG_GENERAL(INFO, "Verify test_downloadFile_deb completed.");
}

BOOST_AUTO_TEST_CASE(test_downloadFile_dsnode) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Running test_downloadFile_dsnode");

  vector<PubKey> dsNode;

  if (!UpgradeManager::GetInstance().LoadInitialDS(dsNode)) {
    LOG_GENERAL(WARNING, "Failed");
    return;
  } else {
    LOG_GENERAL(INFO, "Success");
  }
  LOG_GENERAL(INFO, "Verify test_downloadFile_dsnode completed.");
}

BOOST_AUTO_TEST_SUITE_END()
