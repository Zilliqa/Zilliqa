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

#include <array>

#include "Mediator.h"
#include "common/Constants.h"
#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"
#include "libValidator/Validator.h"

using namespace std;

Mediator::Mediator(const pair<PrivKey, PubKey>& key, const Peer& peer)
    : m_selfKey(key)
    , m_selfPeer(peer)
{
    m_ds = nullptr;
    m_node = nullptr;
    m_validator = nullptr;
    m_currentEpochNum = 0;
    m_isRetrievedHistory = false;
}

Mediator::~Mediator() {}

void Mediator::RegisterColleagues(DirectoryService* ds, Node* node,
                                  Lookup* lookup, ValidatorBase* validator)
{
    m_ds = ds;
    m_node = node;
    m_lookup = lookup;
    m_validator = validator;
}

void Mediator::UpdateDSBlockRand(bool isGenesis)
{
    LOG_MARKER();

    if (isGenesis)
    {
        //genesis block
        LOG_GENERAL(INFO, "Genesis DSBlockchain")
        array<unsigned char, UINT256_SIZE> rand1;
        rand1 = DataConversion::HexStrToStdArray(RAND1_GENESIS);
        copy(rand1.begin(), rand1.end(), m_dsBlockRand.begin());
    }
    else
    {
        DSBlock lastBlock = m_dsBlockChain.GetLastBlock();
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
        vector<unsigned char> vec;
        lastBlock.GetHeader().Serialize(vec, 0);
        sha2.Update(vec);
        vector<unsigned char> randVec;
        randVec = sha2.Finalize();
        copy(randVec.begin(), randVec.end(), m_dsBlockRand.begin());
    }
}

void Mediator::UpdateTxBlockRand(bool isGenesis)
{
    LOG_MARKER();

    if (isGenesis)
    {
        LOG_GENERAL(INFO, "Genesis txBlockchain")
        array<unsigned char, UINT256_SIZE> rand2;
        rand2 = DataConversion::HexStrToStdArray(RAND2_GENESIS);
        copy(rand2.begin(), rand2.end(), m_txBlockRand.begin());
    }
    else
    {
        TxBlock lastBlock = m_txBlockChain.GetLastBlock();
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
        vector<unsigned char> vec;
        lastBlock.GetHeader().Serialize(vec, 0);
        sha2.Update(vec);
        vector<unsigned char> randVec;
        randVec = sha2.Finalize();
        copy(randVec.begin(), randVec.end(), m_txBlockRand.begin());
    }
}

std::string Mediator::GetNodeMode(const Peer& peer)
{
    std::lock_guard<mutex> lock(m_mutexDSCommittee);
    bool bFound = false;

    for (auto const& i : m_DSCommittee)
    {
        if (i.second == peer)
        {
            bFound = true;
            break;
        }
    }

    if (bFound)
    {
        if (peer == m_DSCommittee[0].second)
        {
            return "DSLD";
        }
        else
        {
            return "DSBU";
        }
    }
    else
    {
        return "SHRD";
    }
}
