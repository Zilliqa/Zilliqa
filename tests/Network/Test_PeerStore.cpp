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

#include "libNetwork/PeerStore.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE peerstoretest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(peerstoretest)

BOOST_AUTO_TEST_CASE(test1) {
  INIT_STDOUT_LOGGER();

  PeerStore& ps = PeerStore::GetStore();

  BOOST_CHECK_MESSAGE(ps.GetPeerCount() == 0,
                      "PeerStore initial state check #1 failed");
  BOOST_CHECK_MESSAGE(ps.GetAllPeers().size() == 0,
                      "PeerStore initial state check #2 failed");

  pair<PrivKey, PubKey> keypair1 = Schnorr::GetInstance().GenKeyPair();
  Peer peer(std::rand(), std::rand());

  ps.AddPeerPair(keypair1.second, peer);
  ps.AddPeerPair(keypair1.second, peer);
  ps.AddPeerPair(keypair1.second, peer);
  BOOST_CHECK_MESSAGE(ps.GetPeerCount() == 1,
                      "PeerStore uniqueness check failed");

  Peer peer2 = ps.GetPeer(keypair1.second);
  BOOST_CHECK_MESSAGE(peer == peer2, "PeerStore AddPeer check #1 failed");

  peer.m_ipAddress++;
  peer.m_listenPortHost--;
  ps.AddPeerPair(keypair1.second, peer);
  BOOST_CHECK_MESSAGE(ps.GetPeerCount() == 1,
                      "PeerStore peer replacement check #1 failed");
  peer2 = ps.GetPeer(keypair1.second);
  BOOST_CHECK_MESSAGE(peer == peer2,
                      "PeerStore peer replacement check #2 failed");

  pair<PrivKey, PubKey> keypair2 = Schnorr::GetInstance().GenKeyPair();
  ps.AddPeerPair(keypair2.second, peer);
  BOOST_CHECK_MESSAGE(ps.GetPeerCount() == 2,
                      "PeerStore AddPeer check #2 failed");
  BOOST_CHECK_MESSAGE(ps.GetAllPeers().size() == 2,
                      "PeerStore AddPeer check #3 failed");

  ps.RemovePeer(keypair1.second);
  peer2 = ps.GetPeer(keypair1.second);
  BOOST_CHECK_MESSAGE((peer2.m_ipAddress == 0) && (peer2.m_listenPortHost == 0),
                      "PeerStore RemovePeer check #1 failed");
  BOOST_CHECK_MESSAGE(ps.GetPeerCount() == 1,
                      "PeerStore RemovePeer check #2 failed");

  ps.RemoveAllPeers();
  BOOST_CHECK_MESSAGE(ps.GetPeerCount() == 0,
                      "PeerStore RemoveAllPeers failed");
}

BOOST_AUTO_TEST_SUITE_END()
