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
*
* This should only be used in testnet release  only. This is to ensure the stability of testnet.
* Mainnet will not require this function and nodes will be incentivise to perform the role
* as member of DS committee.
**/

#include "Whitelist.h"

#include <arpa/inet.h>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <iostream>

#include "Peer.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

Whitelist::Whitelist() {}

Whitelist::~Whitelist() {}

Whitelist& Whitelist::GetInstance()
{
    static Whitelist whitelistInfo;
    return whitelistInfo;
}

void Whitelist::AddToDSWhitelist()
{
    ifstream config("whitelist.xml");
    if (config.fail())
    {
        LOG_GENERAL(WARNING, "No whitelist xml present");
    }

    using boost::property_tree::ptree;
    ptree pt;
    read_xml(config, pt);

    BOOST_FOREACH (ptree::value_type const& v, pt.get_child("nodes"))
    {
        if (v.first == "peer")
        {
            PubKey key(
                DataConversion::HexStrToUint8Vec(v.second.get<string>("pubk")),
                0);

            struct in_addr ip_addr;
            inet_aton(v.second.get<string>("ip").c_str(), &ip_addr);
            Peer peer((uint128_t)ip_addr.s_addr,
                      v.second.get<unsigned int>("port"));

            AddToDSWhitelist(peer, key);
        }
    }
    config.close();
}

void Whitelist::AddToDSWhitelist(Peer whiteListPeer, PubKey whiteListPubKey)
{
    lock_guard<mutex> g(m_mutexDSWhiteList);
    m_DSWhiteList.insert(make_pair(whiteListPeer, whiteListPubKey));
    LOG_GENERAL(INFO, "Added " << whiteListPeer << " " << whiteListPubKey);
}

bool Whitelist::IsNodeInDSWhiteList(Peer nodeNetworkInfo, PubKey nodePubKey)
{
    lock_guard<mutex> g(m_mutexDSWhiteList);
    if (m_DSWhiteList.find(nodeNetworkInfo) == m_DSWhiteList.end())
    {
        LOG_GENERAL(INFO,
                    "Not inside whitelist " << nodeNetworkInfo << " "
                                            << nodePubKey);
        return false;
    }

    if (m_DSWhiteList.at(nodeNetworkInfo) == nodePubKey)
    {
        return true;
    }
    else
    {
        LOG_GENERAL(INFO,
                    "Not inside whitelist " << nodeNetworkInfo << " "
                                            << nodePubKey);
        return false;
    }
}
