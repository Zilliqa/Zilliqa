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

#include "ReputationManager.h"
#include "Blacklist.h"

ReputationManager::ReputationManager()
{
    m_Reputations
        = std::unordered_map<boost::multiprecision::uint128_t, int32_t,
                             hash_str<boost::multiprecision::uint128_t>>();
}

ReputationManager::~ReputationManager() {}

ReputationManager& ReputationManager::GetInstance()
{
    static ReputationManager RM;
    return RM;
}

bool ReputationManager::IsNodeBanned(boost::multiprecision::uint128_t IPAddress)
{
    AddNodeIfNotKnown(IPAddress);
    if (GetReputation(IPAddress) <= REPTHRASHHOLD)
    {
        return true;
    }
    return false;
}

void ReputationManager::PunishNode(boost::multiprecision::uint128_t IPAddress)
{
    AddNodeIfNotKnown(IPAddress);
    UpdateReputation(IPAddress, AWARD_FOR_GOOD_NODES);
    if (!Blacklist::GetInstance().Exist(IPAddress) and IsNodeBanned(IPAddress))
    {
        LOG_GENERAL(INFO, "Node " << IPAddress << " banned.");
        Blacklist::GetInstance().Add(IPAddress);
    }
}

void ReputationManager::AwardNode(boost::multiprecision::uint128_t IPAddress)
{
    AddNodeIfNotKnown(IPAddress);
    UpdateReputation(IPAddress, AWARD_FOR_GOOD_NODES);

    if (Blacklist::GetInstance().Exist(IPAddress) and !IsNodeBanned(IPAddress))
    {
        LOG_GENERAL(INFO, "Node " << IPAddress << " unbanned.");
        Blacklist::GetInstance().Remove(IPAddress);
    }
}

void ReputationManager::AddNodeIfNotKnown(
    boost::multiprecision::uint128_t IPAddress)
{
    std::lock_guard<std::mutex> lock(m_mutexReputations);
    if (m_Reputations.find(IPAddress) == m_Reputations.end())
    {
        m_Reputations.emplace(IPAddress, ReputationManager::GOOD);
    }
}

int32_t
ReputationManager::GetReputation(boost::multiprecision::uint128_t IPAddress)
{
    AddNodeIfNotKnown(IPAddress);
    std::lock_guard<std::mutex> lock(m_mutexReputations);
    return m_Reputations[IPAddress];
}

void ReputationManager::SetReputation(
    boost::multiprecision::uint128_t IPAddress, int32_t ReputationScore)
{
    AddNodeIfNotKnown(IPAddress);

    std::lock_guard<std::mutex> lock(m_mutexReputations);
    if (ReputationScore > UPPERREPTHRASHHOLD)
    {
        LOG_GENERAL(
            WARNING,
            "Reputation score too high. Exceed upper bound. ReputationScore: "
                << ReputationScore << ". Setting reputation to "
                << UPPERREPTHRASHHOLD);
        m_Reputations[IPAddress] = UPPERREPTHRASHHOLD;
        return;
    }

    m_Reputations[IPAddress] = ReputationScore;
}

void ReputationManager::UpdateReputation(
    boost::multiprecision::uint128_t IPAddress, int32_t ReputationScoreDelta)
{
    AddNodeIfNotKnown(IPAddress);

    // TODO: check overflow
    int32_t NewRep = GetReputation(IPAddress) + ReputationScoreDelta;
    if (NewRep && !IsNodeBanned(IPAddress))
    {
        NewRep -= BAN_MULTIPLIER * AWARD_FOR_GOOD_NODES;
    }
    SetReputation(IPAddress, NewRep);
}
