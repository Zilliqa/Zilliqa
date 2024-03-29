/*
	This file is part of cpp-ethereum.
	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file MemoryDB.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */
// adapted from https://github.com/ethereum/cpp-ethereum/blob/develop/libdevcore/MemoryDB.h

#ifndef __MEMORYDB_H__
#define __MEMORYDB_H__

#include <shared_mutex>
#include <unordered_map>
#include <memory>

#include "depends/common/Common.h"
#include "depends/common/RLP.h"

namespace dev
{
    class MemoryDB
    {
        friend class EnforceRefs;

    public:
        MemoryDB() {
            m_main.reset(new std::unordered_map<h256, std::pair<std::string, unsigned>>());
        }
        MemoryDB(MemoryDB const& _c) { operator=(_c); }
        explicit MemoryDB(const std::string & _s, bool keepHistory = false) {
            m_main = std::make_shared<std::unordered_map<h256, std::pair<std::string, unsigned>>>();
        }

        MemoryDB& operator=(MemoryDB const& _c);

        void clear() { 
            m_main->clear();
            m_main.reset(new std::unordered_map<h256, std::pair<std::string, unsigned>>()); 
            m_aux.clear();
        }	// WARNING !!!! didn't originally clear m_refCount!!!
        std::unordered_map<h256, std::string> get() const;

        std::string lookup(h256 const& _h) const;
        bool exists(h256 const& _h) const;
        void insert(h256 const& _h, zbytesConstRef _v);
        bool kill(h256 const& _h);

        zbytes lookupAux(h256 const& _h) const;
        void removeAux(h256 const& _h);
        void insertAux(h256 const& _h, zbytesConstRef _v);

        h256Hash keys() const;

    protected:
// #if DEV_GUARDED_DB
        mutable std::shared_timed_mutex x_this;
// #endif
        std::shared_ptr<std::unordered_map<h256, std::pair<std::string, unsigned>>> m_main;
        std::unordered_map<h256, std::pair<zbytes, bool>> m_aux;

        mutable bool m_enforceRefs = false;

        /// remove nodes if counter drop to 0 and collect their keys
        /// set calledWithMutex to false if you *really* know what you are doing
        void purge(std::vector<h256>& purged, bool calledWithMutex = true);
        void purgeMain(std::vector<h256>& purged);
    };

    class EnforceRefs
    {
    public:
        EnforceRefs(MemoryDB const& _o, bool _r): m_o(_o), m_r(_o.m_enforceRefs) { _o.m_enforceRefs = _r; }
        ~EnforceRefs() { m_o.m_enforceRefs = m_r; }

    private:
        MemoryDB const& m_o;
        bool m_r;
    };

    inline std::ostream& operator<<(std::ostream& _out, MemoryDB const& _m)
    {
        for (auto const& i: _m.get())
        {
            _out << i.first << ": ";
            _out << RLP(i.second);
            _out << " " << toHex(i.second);
            _out << std::endl;
        }
        return _out;
    }
}

#endif // __MEMORYDB_H__
