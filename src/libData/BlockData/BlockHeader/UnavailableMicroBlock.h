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

#ifndef __UNAVAILABLEMICROBLOCK_H__
#define __UNAVAILABLEMICROBLOCK_H__

#include "BlockHashSet.h"

struct UnavailableMicroBlock
{
    MicroBlockHashSet m_hash;
    uint32_t m_shardID;

    bool operator==(const UnavailableMicroBlock& umb) const
    {
        return std::tie(m_hash, m_shardID)
            == std::tie(umb.m_hash, umb.m_shardID);
    }

    bool operator<(const UnavailableMicroBlock& umb) const
    {
        return std::tie(umb.m_hash, umb.m_shardID)
            > std::tie(m_hash, m_shardID);
    }

    bool operator>(const UnavailableMicroBlock& umb) const
    {
        return !((*this == umb) || (*this < umb));
    }

    friend std::ostream& operator<<(std::ostream& os,
                                    const UnavailableMicroBlock& t);
};

inline std::ostream& operator<<(std::ostream& os,
                                const UnavailableMicroBlock& t)
{
    os << "m_txRootHash : " << t.m_hash.m_txRootHash.hex() << std::endl
       << "m_stateDeltaHash : " << t.m_hash.m_stateDeltaHash.hex() << std::endl
       << "m_shardID : " << t.m_shardID;
    return os;
}

// define its hash function in order to used as key in map
namespace std
{
    template<> struct hash<UnavailableMicroBlock>
    {
        size_t operator()(UnavailableMicroBlock const& umb) const noexcept
        {
            size_t const h1(std::hash<MicroBlockHashSet>{}(umb.m_hash));
            size_t const h2(std::hash<uint32_t>{}(umb.m_shardID));
            return h1 ^ (h2 << 1);
        }
    };
}

#endif // __UNAVAILABLEMICROBLOCK_H__