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

#include <boost/filesystem.hpp>

#include <string>
#include <vector>
#include "common/Constants.h"
#include "libData/MiningData/DSPowSolution.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include "libTestUtils/TestUtils.h"

#define BOOST_TEST_MODULE dspowsolutiontest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

BOOST_AUTO_TEST_SUITE(dspowsolutiontest)

BOOST_AUTO_TEST_CASE(testContractInvoking) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();



}

BOOST_AUTO_TEST_SUITE_END()
