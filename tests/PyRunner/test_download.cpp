/*
 * Copyright (C) 2020 Zilliqa
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

#include <arpa/inet.h>
#include <array>
#include <string>
#include <thread>
#include <vector>

#include <Schnorr.h>
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libMediator/Mediator.h"
#include "libNode/Node.h"

#define BOOST_TEST_MODULE testdownloadpy
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(testdownloadpy)

BOOST_AUTO_TEST_CASE(testpyrunner) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  PairOfKey key;
  Peer peer;

  Mediator mediator(key, peer);
  Node node(mediator, 0, false);
  Lookup lk(mediator, NO_SYNC);
  auto vd = make_shared<Validator>(mediator);

  mediator.RegisterColleagues(nullptr, &node, &lk, vd.get());

  node.DownloadPersistenceFromS3();
}

BOOST_AUTO_TEST_SUITE_END()