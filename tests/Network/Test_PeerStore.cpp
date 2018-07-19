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

#include "libNetwork/PeerStore.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE peerstoretest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(peerstoretest)

BOOST_AUTO_TEST_CASE(test1)
{
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
    BOOST_CHECK_MESSAGE((peer2.m_ipAddress == 0)
                            && (peer2.m_listenPortHost == 0),
                        "PeerStore RemovePeer check #1 failed");
    BOOST_CHECK_MESSAGE(ps.GetPeerCount() == 1,
                        "PeerStore RemovePeer check #2 failed");

    ps.RemoveAllPeers();
    BOOST_CHECK_MESSAGE(ps.GetPeerCount() == 0,
                        "PeerStore RemoveAllPeers failed");
}

BOOST_AUTO_TEST_SUITE_END()
