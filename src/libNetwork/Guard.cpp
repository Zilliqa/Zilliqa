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

#include "Guard.h"

#include <arpa/inet.h>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <cstring>
#include <iostream>
#include <string>

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
      PubKey pubKey(DataConversion::HexStrToUint8Vec(v.second.data()), 0);
      AddToDSGuardlist(pubKey);
    }
  }

  {
    lock_guard<mutex> g(m_mutexDSGuardList);
    LOG_GENERAL(INFO, "Total number of entries in DS guard list:  "
                          << m_DSGuardList.size());
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
      PubKey pubKey(DataConversion::HexStrToUint8Vec(v.second.data()), 0);
      AddToShardGuardlist(pubKey);
    }
  }
  {
    lock_guard<mutex> g(m_mutexShardGuardList);
    LOG_GENERAL(INFO, "Total number of entries in shard guard list:  "
                          << m_ShardGuardList.size());
  }
}

void Guard::AddToDSGuardlist(const PubKey& dsGuardPubKey) {
  if (!GUARD_MODE) {
    LOG_GENERAL(WARNING, "Not in Guard mode. Guard list is not available.");
    return;
  }

  lock_guard<mutex> g(m_mutexDSGuardList);
  m_DSGuardList.emplace_back(dsGuardPubKey);
  // LOG_GENERAL(INFO, "Added " << dsGuardPubKey);
}

void Guard::AddToShardGuardlist(const PubKey& shardGuardPubKey) {
  if (!GUARD_MODE) {
    LOG_GENERAL(WARNING, "Not in Guard mode. Guard list is not available.");
    return;
  }

  lock_guard<mutex> g(m_mutexShardGuardList);
  m_ShardGuardList.emplace_back(shardGuardPubKey);
}

bool Guard::IsNodeInDSGuardList(const PubKey& nodePubKey) {
  if (!GUARD_MODE) {
    LOG_GENERAL(WARNING, "Not in Guard mode. DS guard is not available.");
    return false;
  }

  lock_guard<mutex> g(m_mutexDSGuardList);
  return (std::find(m_DSGuardList.begin(), m_DSGuardList.end(), nodePubKey) !=
          m_DSGuardList.end());
}

bool Guard::IsNodeInShardGuardList(const PubKey& nodePubKey) {
  if (!GUARD_MODE) {
    LOG_GENERAL(WARNING, "Not in Guard mode. Shard guard is not available.");
    return false;
  }

  lock_guard<mutex> g(m_mutexShardGuardList);
  return (std::find(m_ShardGuardList.begin(), m_ShardGuardList.end(),
                    nodePubKey) != m_ShardGuardList.end());
}

unsigned int Guard::GetNumOfDSGuard() {
  lock_guard<mutex> g(m_mutexDSGuardList);
  return m_DSGuardList.size();
}
unsigned int Guard::GetNumOfShardGuard() {
  lock_guard<mutex> g(m_mutexShardGuardList);
  return m_ShardGuardList.size();
}

// This feature is only available to ds guard nodes. This only guard nodes to
// change its network information. Pre-condition: Must still have access to
// existing public and private key pair
bool Guard::UpdateDSGuardIdentity(Mediator& mediator) {
  if (!GUARD_MODE) {
    LOG_GENERAL(
        WARNING,
        "Not in guard mode. Unable to update ds guard network identity.");
    return false;
  }

  // To provide current pubkey, new IP, new Port and current timestamp
  vector<unsigned char> updatedsguardidentitymessage = {
      MessageType::DIRECTORY, DSInstructionType::POWSUBMISSION};

  if (!Messenger::SetDSLookupNewDSGuardNetworkInfo(
          updatedsguardidentitymessage, MessageOffset::BODY,
          mediator.m_currentEpochNum, mediator.m_selfPeer, get_time_as_int(),
          mediator.m_selfKey)) {
    LOG_EPOCH(WARNING, to_string(mediator.m_currentEpochNum).c_str(),
              "Messenger::SetDSLookupNewDSGuardNetworkInfo failed.");
    return false;
  }

  // Broadcast to lookup
  // mediator.m_lookup->SendMessageToLookupNodes(updatedsguardidentitymessage);

  {
    // Gossip to all DS committee
    lock_guard<mutex> lock(mediator.m_mutexDSCommittee);
    deque<Peer> peerInfo;

    for (auto const& i : *mediator.m_DSCommittee) {
      peerInfo.push_back(i.second);
    }

    if (BROADCAST_GOSSIP_MODE) {
      P2PComm::GetInstance().SpreadRumor(updatedsguardidentitymessage);
    } else {
      P2PComm::GetInstance().SendMessage(peerInfo,
                                         updatedsguardidentitymessage);
    }
  }

  return true;
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

void Guard::ValidateRunTimeEnvironment() {
  unsigned int nodeReplacementLimit =
      COMM_SIZE - ceil(COMM_SIZE * ConsensusCommon::TOLERANCE_FRACTION);

  if (NUM_DS_ELECTION > nodeReplacementLimit) {
    LOG_GENERAL(FATAL,
                "Check constants configuration. nodeReplacementLimit must be "
                "bigger than NUM_DS_ELECTION. Refer to design documentation. "
                "nodeReplacementLimit: "
                    << nodeReplacementLimit);
  } else {
    LOG_GENERAL(INFO, "Passed guard mode run time enviornment validation");
  }
}

void Guard::Init() {
  if (GUARD_MODE) {
    LOG_GENERAL(INFO, "In Guard mode. Updating DS and Shard guard lists");
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
