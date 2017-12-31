/**
* Copyright (c) 2017 Zilliqa 
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

#include "Zilliqa.h"
#include "common/Messages.h"
#include "libUtils/Logger.h"

using namespace std;

Zilliqa::Zilliqa(const std::pair<PrivKey, PubKey> & key, const Peer & peer, bool loadConfig) :
        m_pm(key, peer, loadConfig), m_mediator(key, peer), m_ds(m_mediator), m_lookup(m_mediator), 
        m_n(m_mediator), m_cu(key, peer)
{
    LOG_MARKER();

    m_mediator.RegisterColleagues(&m_ds, &m_n, &m_lookup);

    vector<unsigned char> tmp1;
    vector<unsigned char> tmp2;

    key.first.Serialize(tmp1, 0);
    key.second.Serialize(tmp2, 0);

    LOG_PAYLOAD("Private Key", tmp1, PRIV_KEY_SIZE*2);
    LOG_PAYLOAD("Public Key", tmp2, PUB_KEY_SIZE*2);

#ifdef STAT_TEST
    P2PComm::GetInstance().SetSelfPeer(peer);
#endif // STAT_TEST
#ifndef IS_LOOKUP_NODE
    m_n.StartSynchronization();
#endif // IS_LOOKUP_NODE
}

Zilliqa::~Zilliqa()
{

}

void Zilliqa::Dispatch(const vector<unsigned char> & message, const Peer & from)
{
    LOG_MARKER();

    if (message.size() >= MessageOffset::BODY)
    {
        const unsigned char msg_type = message.at(MessageOffset::TYPE);

        Executable * msg_handlers[] =
        {
            &m_pm,
            &m_ds,
            &m_n,
            &m_cu,
            &m_lookup
        };

        const unsigned int msg_handlers_count = sizeof(msg_handlers) / sizeof(Executable*);

        if (msg_type < msg_handlers_count)
        {
            bool result = msg_handlers[msg_type]->Execute(message, MessageOffset::INST, from);

            if (result == false)
            {
                // To-do: Error recovery
            }
        }
        else
        {
            LOG_MESSAGE("Unknown message type " << std::hex << (unsigned int)msg_type);
        }
    }
}

vector<Peer> Zilliqa::RetrieveBroadcastList(unsigned char msg_type, unsigned char ins_type, const Peer & from)
{
    LOG_MARKER();

    Broadcastable * msg_handlers[] =
    {
        &m_pm,
        &m_ds,
        &m_n
    };

    const unsigned int msg_handlers_count = sizeof(msg_handlers) / sizeof(Broadcastable*);

    if (msg_type < msg_handlers_count)
    {
        return msg_handlers[msg_type]->GetBroadcastList(ins_type, from);
    }
    else
    {
        LOG_MESSAGE("Unknown message type " << std::hex << (unsigned int)msg_type);
    }

    return vector<Peer>();
}
