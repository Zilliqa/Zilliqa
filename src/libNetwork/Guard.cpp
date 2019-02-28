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

#include "Guard.h"

#include <arpa/inet.h>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <cstring>
#include <iostream>
#include <string>

#include "Blacklist.h"
#include "Peer.h"
#include "common/Messages.h"
#include "libConsensus/ConsensusCommon.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

Guard::Guard() {}

Guard::~Guard() {}

Guard& Guard::GetInstance() {
  static Guard guardInstance;
  return guardInstance;
}

void Guard::UpdateDSGuardlist() {
  if (!GUARD_MODE) {
    LOG_GENERAL(WARNING, "Not in Guard mode. DS guard is not available.");
    return;
  }

  ifstream config("constants.xml");

  if (config.fail()) {
    LOG_GENERAL(WARNING, "No constants xml present");
    return;
  }

  using boost::property_tree::ptree;
  ptree pt;
  read_xml(config, pt);

  for (const ptree::value_type& v : pt.get_child("node.ds_guard")) {
    if (v.first == "DSPUBKEY") {
      bytes pubkeyBytes;
      if (!DataConversion::HexStrToUint8Vec(v.second.data(), pubkeyBytes)) {
        continue;
      }
      PubKey pubKey(pubkeyBytes, 0);
      AddToDSGuardlist(pubKey);
    }
  }

  {
    lock_guard<mutex> g(m_mutexDSGuardList);
    LOG_GENERAL(INFO, "Entries = " << m_DSGuardList.size());
  }
}

void Guard::UpdateShardGuardlist() {
  if (!GUARD_MODE) {
    LOG_GENERAL(WARNING, "Not in guard mode. Guard list is not available.");
    return;
  }

  ifstream config("constants.xml");

  if (config.fail()) {
    LOG_GENERAL(WARNING, "No constants xml present");
    return;
  }

  using boost::property_tree::ptree;
  ptree pt;
  read_xml(config, pt);

  for (const ptree::value_type& v : pt.get_child("node.shard_guard")) {
    if (v.first == "SHARDPUBKEY") {
      bytes pubkeyBytes;
      if (!DataConversion::HexStrToUint8Vec(v.second.data(), pubkeyBytes)) {
        continue;
      }
      PubKey pubKey(pubkeyBytes, 0);
      AddToShardGuardlist(pubKey);
    }
  }
  {
    lock_guard<mutex> g(m_mutexShardGuardList);
    LOG_GENERAL(INFO, "Entries = " << m_ShardGuardList.size());
  }
}

void Guard::AddToDSGuardlist(const PubKey& dsGuardPubKey) {
  if (!GUARD_MODE) {
    LOG_GENERAL(WARNING, "Not in Guard mode. Guard list is not available.");
    return;
  }

  lock_guard<mutex> g(m_mutexDSGuardList);
  m_DSGuardList.emplace(dsGuardPubKey);
  // LOG_GENERAL(INFO, "Added " << dsGuardPubKey);
}

void Guard::AddToShardGuardlist(const PubKey& shardGuardPubKey) {
  if (!GUARD_MODE) {
    LOG_GENERAL(WARNING, "Not in Guard mode. Guard list is not available.");
    return;
  }

  lock_guard<mutex> g(m_mutexShardGuardList);
  m_ShardGuardList.emplace(shardGuardPubKey);
}

bool Guard::IsNodeInDSGuardList(const PubKey& nodePubKey) {
  if (!GUARD_MODE) {
    LOG_GENERAL(WARNING, "Not in Guard mode. DS guard is not available.");
    return false;
  }

  lock_guard<mutex> g(m_mutexDSGuardList);
  return (m_DSGuardList.find(nodePubKey) != m_DSGuardList.end());
}

bool Guard::IsNodeInShardGuardList(const PubKey& nodePubKey) {
  if (!GUARD_MODE) {
    LOG_GENERAL(WARNING, "Not in Guard mode. Shard guard is not available.");
    return false;
  }

  lock_guard<mutex> g(m_mutexShardGuardList);
  return (m_ShardGuardList.find(nodePubKey) != m_ShardGuardList.end());
}

unsigned int Guard::GetNumOfDSGuard() {
  lock_guard<mutex> g(m_mutexDSGuardList);
  return m_DSGuardList.size();
}
unsigned int Guard::GetNumOfShardGuard() {
  lock_guard<mutex> g(m_mutexShardGuardList);
  return m_ShardGuardList.size();
}

bool Guard::IsValidIP(const uint128_t& ip_addr) {
  struct sockaddr_in serv_addr;
  serv_addr.sin_addr.s_addr = ip_addr.convert_to<unsigned long>();
  uint32_t ip_addr_c = ntohl(serv_addr.sin_addr.s_addr);
  if (ip_addr <= 0 || ip_addr >= (uint32_t)-1) {
    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(serv_addr.sin_addr), ipStr, INET_ADDRSTRLEN);

    LOG_GENERAL(WARNING, "Invalid IPv4 address " << string(ipStr));
    return false;
  }

  if (!EXCLUDE_PRIV_IP) {
    // No filtering enable. Hence, IP (other than 0.0.0.0 and 255.255.255.255)
    // is allowed.
    return true;
  }

  lock_guard<mutex> g(m_mutexIPExclusion);
  for (const auto& ip_pair : m_IPExclusionRange) {
    if (ip_pair.first <= ip_addr_c && ip_pair.second >= ip_addr_c) {
      char ipStr[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(serv_addr.sin_addr), ipStr, INET_ADDRSTRLEN);

      LOG_GENERAL(WARNING, "In Exclusion List: " << string(ipStr));
      return false;
    }
  }

  return true;
}

void Guard::AddToExclusionList(const string& ft, const string& sd) {
  struct sockaddr_in serv_addr1, serv_addr2;
  try {
    inet_pton(AF_INET, ft.c_str(), &serv_addr1.sin_addr);
    inet_pton(AF_INET, sd.c_str(), &serv_addr2.sin_addr);
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Error " << e.what());
    return;
  }

  AddToExclusionList(serv_addr1.sin_addr.s_addr, serv_addr2.sin_addr.s_addr);
}

void Guard::AddToExclusionList(const uint128_t& ft, const uint128_t& sd) {
  if (ft > (uint32_t)-1 || sd > (uint32_t)-1) {
    LOG_GENERAL(WARNING, "Wrong parameters for IPv4");
    return;
  }
  uint32_t ft_c = ntohl(ft.convert_to<uint32_t>());
  uint32_t sd_c = ntohl(sd.convert_to<uint32_t>());
  lock_guard<mutex> g(m_mutexIPExclusion);

  if (ft_c > sd_c) {
    m_IPExclusionRange.emplace_back(sd_c, ft_c);
  } else {
    m_IPExclusionRange.emplace_back(ft_c, sd_c);
  }
}

void Guard::AddDSGuardToBlacklistExcludeList(const DequeOfNode& dsComm) {
  if (GUARD_MODE) {
    unsigned int dsIndex = 0;
    for (const auto& i : dsComm) {
      if (dsIndex < GetNumOfDSGuard()) {
        // Ensure it is not 0.0.0.0
        if (IsNodeInDSGuardList(i.first) && i.second.m_ipAddress != 0) {
          Blacklist::GetInstance().Exclude(i.second.m_ipAddress);
          LOG_GENERAL(INFO,
                      "Excluding ds guard " << i.second << " from blacklist");
        } else {
          LOG_GENERAL(WARNING,
                      "Unable to exclude " << i.second << " from blacklist");
        }
        dsIndex++;
      } else {
        break;
      }
    }
  }
}

void Guard::ValidateRunTimeEnvironment() {
  LOG_MARKER();

  unsigned int nodeReplacementLimit =
      COMM_SIZE - ceil(COMM_SIZE * ConsensusCommon::TOLERANCE_FRACTION);

  if (NUM_DS_ELECTION > nodeReplacementLimit) {
    LOG_GENERAL(FATAL,
                "Check constants configuration. nodeReplacementLimit must be "
                "bigger than NUM_DS_ELECTION. Refer to design documentation. "
                "nodeReplacementLimit: "
                    << nodeReplacementLimit);
  }
}

void Guard::Init() {
  if (GUARD_MODE) {
    LOG_GENERAL(INFO, "Updating lists");
    ValidateRunTimeEnvironment();
    UpdateDSGuardlist();
    UpdateShardGuardlist();
  }

  if (EXCLUDE_PRIV_IP) {
    LOG_GENERAL(INFO, "Adding Priv IPs to Exclusion List");
    AddToExclusionList("172.16.0.0", "172.31.255.255");
    AddToExclusionList("192.168.0.0", "192.168.255.255");
    AddToExclusionList("10.0.0.0", "10.255.255.255");
  }
}
