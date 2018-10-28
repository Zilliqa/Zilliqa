/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#include "common/Constants.h"
#include "libUtils/Logger.h"
#include "libUtils/ShardSizeCalculator.h"

#define BOOST_TEST_MODULE ShardSizeCalculator
#define BOOST_TEST_DYN_LINK
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/test/unit_test.hpp>
#include <map>
#include <array>
#include <vector>

using namespace std;

typedef std::vector<uint32_t> TestDataSet;
typedef std::map<uint32_t, TestDataSet> ShardSizeMap;

void prepareTestdata(ShardSizeMap &testData){
	TestDataSet tds;
	testData[651] = tds = {0, 1, 650, 651, 1000, 1367};
	testData[684] = tds = {1368, 2131, 2132};
	testData[711] = tds = {2133, 2200, 2867};;
	testData[717] = tds = {2868, 2869, 3674};
	testData[735] = tds = {3675, 4000, 4463};
	testData[744] = tds = {4464, 5000, 5228};
	testData[747] = tds = {5229, 6000, 6023};
	testData[753] = tds = {6024, 6800, 6857};
	testData[762] = tds = {6858, 7000, 7709};
	testData[771] = tds = {7710, 8000, 8579};
	testData[780] = tds = {8580, 9000, 9467};
	testData[789] = tds = {9468, 10000, 10334};
	testData[795] = tds = {10335, 11000, 14363};
	testData[798] = tds = {14364,15000,15389};
	testData[810] = tds = {15390,16000,18767};
	testData[816] = tds = {18768,19000,20399};
	testData[819] = tds = {20400,300000,std::numeric_limits<uint32_t>::max()};
}

BOOST_AUTO_TEST_SUITE(shardsizecalculator)

#define TD_i td_i
#define EXPECTED td_i.first
#define NUMOFNODES_v td_i.second

BOOST_AUTO_TEST_CASE(test_lower_bound) {

  INIT_STDOUT_LOGGER();
  ShardSizeMap testData;
  prepareTestdata(testData);
  for (auto const& TD_i : testData)
  {
	for(auto const& numOfNodes: NUMOFNODES_v){
		uint32_t result = ShardSizeCalculator::CalculateShardSize(numOfNodes);
	  	BOOST_CHECK_MESSAGE(result == EXPECTED,
	  			"For number of nodes: " + to_string(numOfNodes) + " Expected: " + to_string(EXPECTED) + ". Result: " + to_string(result));
	}
  }
}

BOOST_AUTO_TEST_SUITE_END()
