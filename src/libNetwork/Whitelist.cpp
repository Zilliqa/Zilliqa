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
#include <string>

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

void Whitelist::UpdateDSWhitelist()
{
    if (!TEST_NET_MODE)
    {
        LOG_GENERAL(WARNING, "Not in testnet mode. Whitelisting not allowed");
        return;
    }

    ifstream config("ds_whitelist.xml");

    if (config.fail())
    {
        LOG_GENERAL(WARNING, "No whitelist xml present");
        return;
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

    LOG_GENERAL(
        INFO,
        "Total number of entries in DS whitelist:  " << m_DSWhiteList.size());
}

void Whitelist::UpdateShardWhitelist()
{
    if (!TEST_NET_MODE)
    {
        LOG_GENERAL(WARNING, "Not in testnet mode. Whitelisting not allowed");
        return;
    }

    ifstream config("shard_whitelist.xml");

    if (config.fail())
    {
        LOG_GENERAL(WARNING, "No shard_whitelist xml present");
        return;
    }

    using boost::property_tree::ptree;
    ptree pt;
    read_xml(config, pt);

    lock_guard<mutex> g(m_mutexShardWhiteList);
    m_ShardWhiteList.clear();

    for (auto& addr : pt.get_child("address"))
    {
        PubKey key(DataConversion::HexStrToUint8Vec(addr.second.data()), 0);
        m_ShardWhiteList.emplace_back(key);
        // LOG_GENERAL(INFO, "Added " << key);
    }

    LOG_GENERAL(INFO,
                "Total number of entries in shard whitelist:  "
                    << m_ShardWhiteList.size());
}

void Whitelist::AddToDSWhitelist(const Peer& whiteListPeer,
                                 const PubKey& whiteListPubKey)
{
    if (!TEST_NET_MODE)
    {
        // LOG_GENERAL(WARNING, "Not in testnet mode. Whitelisting not allowed");
        return;
    }

    lock_guard<mutex> g(m_mutexDSWhiteList);
    m_DSWhiteList.emplace(whiteListPeer, whiteListPubKey);
    // LOG_GENERAL(INFO, "Added " << whiteListPeer << " " << whiteListPubKey);
}

bool Whitelist::IsNodeInDSWhiteList(const Peer& nodeNetworkInfo,
                                    const PubKey& nodePubKey)
{
    lock_guard<mutex> g(m_mutexDSWhiteList);
    if (m_DSWhiteList.find(nodeNetworkInfo) == m_DSWhiteList.end())
    {
        LOG_GENERAL(WARNING,
                    "Node not inside whitelist " << nodeNetworkInfo << " "
                                                 << nodePubKey);
        return false;
    }

    if (m_DSWhiteList.at(nodeNetworkInfo) == nodePubKey)
    {
        return true;
    }

    LOG_GENERAL(WARNING,
                "Node not inside whitelist " << nodeNetworkInfo << " "
                                             << nodePubKey);
    return false;
}

bool Whitelist::IsPubkeyInShardWhiteList(const PubKey& nodePubKey)
{
    lock_guard<mutex> g(m_mutexShardWhiteList);

    if (std::find(m_ShardWhiteList.begin(), m_ShardWhiteList.end(), nodePubKey)
        == m_ShardWhiteList.end())
    {
        LOG_GENERAL(WARNING, "Pubk Not inside whitelist " << nodePubKey);
        return false;
    }

    return true;
}

bool Whitelist::IsValidIP(const uint128_t& ip_addr)
{
    struct sockaddr_in serv_addr;
    serv_addr.sin_addr.s_addr = ip_addr.convert_to<unsigned long>();
    uint32_t ip_addr_c = ntohl(serv_addr.sin_addr.s_addr);
    if (ip_addr <= 0 || ip_addr >= (uint32_t)-1)
    {
        LOG_GENERAL(WARNING,
                    "Invalid IPv4 address " << inet_ntoa(serv_addr.sin_addr));
        return false;
    }

    if (!EXCLUDE_PRIV_IP)
    {
        // No filtering enable. Hence, IP (other than 0.0.0.0 and 255.255.255.255) is allowed.
        return true;
    }

    lock_guard<mutex> g(m_mutexIPExclusion);
    for (const auto& ip_pair : m_IPExclusionRange)
    {
        if (ip_pair.first <= ip_addr_c && ip_pair.second >= ip_addr_c)
        {
            LOG_GENERAL(WARNING,
                        "In Exclusion List: " << inet_ntoa(serv_addr.sin_addr));
            return false;
        }
    }

    return true;
}

void Whitelist::AddToExclusionList(const string& ft, const string& sd)
{
    struct sockaddr_in serv_addr1, serv_addr2;
    try
    {
        inet_aton(ft.c_str(), &serv_addr1.sin_addr);
        inet_aton(sd.c_str(), &serv_addr2.sin_addr);
    }
    catch (exception& e)
    {
        LOG_GENERAL(WARNING, "Error " << e.what());
        return;
    }

    AddToExclusionList(serv_addr1.sin_addr.s_addr, serv_addr2.sin_addr.s_addr);
}

void Whitelist::AddToExclusionList(const uint128_t& ft, const uint128_t& sd)
{
    if (ft > (uint32_t)-1 || sd > (uint32_t)-1)
    {
        LOG_GENERAL(WARNING, "Wrong parameters for IPv4");
        return;
    }
    uint32_t ft_c = ntohl(ft.convert_to<uint32_t>());
    uint32_t sd_c = ntohl(sd.convert_to<uint32_t>());
    lock_guard<mutex> g(m_mutexIPExclusion);

    if (ft_c > sd_c)
    {
        m_IPExclusionRange.emplace_back(sd_c, ft_c);
    }
    else
    {
        m_IPExclusionRange.emplace_back(ft_c, sd_c);
    }
}

void Whitelist::Init()
{
    UpdateDSWhitelist();
    if (EXCLUDE_PRIV_IP)
    {
        LOG_GENERAL(INFO, "Adding Priv IPs to Exclusion List");
        AddToExclusionList("172.16.0.0", "172.31.255.255");
        AddToExclusionList("192.168.0.0", "192.168.255.255");
        AddToExclusionList("10.0.0.0", "10.255.255.255");
    }
}