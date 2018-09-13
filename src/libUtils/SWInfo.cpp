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

#include "SWInfo.h"
#include "libUtils/Logger.h"

using namespace std;

SWInfo::SWInfo()
    : m_major(0)
    , m_minor(0)
    , m_fix(0)
    , m_upgradeDS(0)
    , m_commit(0)
{
}

SWInfo::SWInfo(const uint32_t& major, const uint32_t& minor,
               const uint32_t& fix, const uint64_t& upgradeDS,
               const uint32_t& commit)
    : m_major(major)
    , m_minor(minor)
    , m_fix(fix)
    , m_upgradeDS(upgradeDS)
    , m_commit(commit)
{
}

SWInfo::SWInfo(const SWInfo& src)
    : m_major(src.m_major)
    , m_minor(src.m_minor)
    , m_fix(src.m_fix)
    , m_upgradeDS(src.m_upgradeDS)
    , m_commit(src.m_commit)
{
}

SWInfo::~SWInfo(){};

/// Implements the Serialize function inherited from Serializable.
unsigned int SWInfo::Serialize(std::vector<unsigned char>& dst,
                               unsigned int offset) const
{
    LOG_MARKER();

    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < SIZE)
    {
        dst.resize(SIZE + offset);
    }

    unsigned int curOffset = offset;

    SetNumber<uint32_t>(dst, curOffset, m_major, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    SetNumber<uint32_t>(dst, curOffset, m_minor, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    SetNumber<uint32_t>(dst, curOffset, m_fix, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    SetNumber<uint64_t>(dst, curOffset, m_upgradeDS, sizeof(uint64_t));
    curOffset += sizeof(uint64_t);
    SetNumber<uint32_t>(dst, curOffset, m_commit, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);

    return SIZE;
}

/// Implements the Deserialize function inherited from Serializable.
int SWInfo::Deserialize(const std::vector<unsigned char>& src,
                        unsigned int offset)
{
    LOG_MARKER();

    unsigned int curOffset = offset;

    try
    {
        m_major = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
        curOffset += sizeof(uint32_t);
        m_minor = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
        curOffset += sizeof(uint32_t);
        m_fix = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
        curOffset += sizeof(uint32_t);
        m_upgradeDS = GetNumber<uint64_t>(src, curOffset, sizeof(uint64_t));
        curOffset += sizeof(uint64_t);
        m_commit = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
        curOffset += sizeof(uint32_t);
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with SWInfo::Deserialize." << ' ' << e.what());
        return -1;
    }

    return 0;
}

/// Less-than comparison operator.
bool SWInfo::operator<(const SWInfo& r) const
{
    return tie(m_major, m_minor, m_fix, m_upgradeDS, m_commit)
        < tie(r.m_major, r.m_minor, r.m_fix, r.m_upgradeDS, r.m_commit);
}

/// Greater-than comparison operator.
bool SWInfo::operator>(const SWInfo& r) const { return r < *this; }

/// Equality operator.
bool SWInfo::operator==(const SWInfo& r) const
{
    return tie(m_major, m_minor, m_fix, m_upgradeDS, m_commit)
        == tie(r.m_major, r.m_minor, r.m_fix, r.m_upgradeDS, r.m_commit);
}
