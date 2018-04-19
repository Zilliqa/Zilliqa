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

#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/server/connectors/httpserver.h>

#include "Zilliqa.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Address.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace jsonrpc;

void Zilliqa::LogSelfNodeInfo(const std::pair<PrivKey, PubKey>& key,
                              const Peer& peer)
{
    vector<unsigned char> tmp1;
    vector<unsigned char> tmp2;

    key.first.Serialize(tmp1, 0);
    key.second.Serialize(tmp2, 0);

    LOG_PAYLOAD(INFO, "Private Key", tmp1, PRIV_KEY_SIZE * 2);
    LOG_PAYLOAD(INFO, "Public Key", tmp2, PUB_KEY_SIZE * 2);

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Reset();
    vector<unsigned char> message;
    key.second.Serialize(message, 0);
    sha2.Update(message, 0, PUB_KEY_SIZE);
    const vector<unsigned char>& tmp3 = sha2.Finalize();
    Address toAddr;
    copy(tmp3.end() - ACC_ADDR_SIZE, tmp3.end(), toAddr.asArray().begin());

    LOG_GENERAL(INFO,
                "My address is " << toAddr << " and port is "
                                 << peer.m_listenPortHost);
}

Zilliqa::Zilliqa(const std::pair<PrivKey, PubKey>& key, const Peer& peer,
                 bool loadConfig, bool toSyncWithNetwork,
                 bool toRetrieveHistory)
    : m_pm(key, peer, loadConfig)
    , m_mediator(key, peer)
    , m_ds(m_mediator)
    , m_lookup(m_mediator)
    , m_n(m_mediator, toRetrieveHistory)
    , m_cu(key, peer)
#ifdef IS_LOOKUP_NODE
    , m_httpserver(SERVER_PORT)
    , m_server(m_mediator, m_httpserver)
#endif // IS_LOOKUP_NODE

{
    LOG_MARKER();

    if (m_mediator.m_isRetrievedHistory)
    {
        m_ds.m_consensusID = 0;
    }

    m_mediator.RegisterColleagues(&m_ds, &m_n, &m_lookup);

    LogSelfNodeInfo(key, peer);

#ifdef STAT_TEST
    P2PComm::GetInstance().SetSelfPeer(peer);
#endif // STAT_TEST

#ifndef IS_LOOKUP_NODE
    LOG_GENERAL(INFO, "I am a normal node.");

    if (toSyncWithNetwork && !toRetrieveHistory)
    {
        m_n.m_runFromLate = true;
        m_n.StartSynchronization();
    }
#else // else for IS_LOOKUP_NODE
    LOG_GENERAL(INFO, "I am a lookup node.");
    if (m_server.StartListening())
    {
        LOG_GENERAL(INFO, "1. API Server started successfully");
    }
    else
    {
        LOG_GENERAL(INFO, "2. API Server couldn't start");
    }
#endif // IS_LOOKUP_NODE
}

Zilliqa::~Zilliqa() {}

void Zilliqa::Dispatch(const vector<unsigned char>& message, const Peer& from)
{
    //LOG_MARKER();

    if (message.size() >= MessageOffset::BODY)
    {
        const unsigned char msg_type = message.at(MessageOffset::TYPE);

        Executable* msg_handlers[] = {&m_pm, &m_ds, &m_n, &m_cu, &m_lookup};

        const unsigned int msg_handlers_count
            = sizeof(msg_handlers) / sizeof(Executable*);

        if (msg_type < msg_handlers_count)
        {
            bool result = msg_handlers[msg_type]->Execute(
                message, MessageOffset::INST, from);

            if (result == false)
            {
                // To-do: Error recovery
            }
        }
        else
        {
            LOG_GENERAL(INFO,
                        "Unknown message type " << std::hex
                                                << (unsigned int)msg_type);
        }
    }
}

vector<Peer> Zilliqa::RetrieveBroadcastList(unsigned char msg_type,
                                            unsigned char ins_type,
                                            const Peer& from)
{
    LOG_MARKER();

    Broadcastable* msg_handlers[] = {&m_pm, &m_ds, &m_n, &m_cu, &m_lookup};

    const unsigned int msg_handlers_count
        = sizeof(msg_handlers) / sizeof(Broadcastable*);

    if (msg_type < msg_handlers_count)
    {
        return msg_handlers[msg_type]->GetBroadcastList(ins_type, from);
    }
    else
    {
        LOG_GENERAL(INFO,
                    "Unknown message type " << std::hex
                                            << (unsigned int)msg_type);
    }

    return vector<Peer>();
}
