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
/** @file OverlayDB.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include <shared_mutex>
#include <thread>

#include <boost/filesystem.hpp>

#include "depends/common/Common.h"
#include "depends/common/SHA3.h"
#include "OverlayDB.h"

using namespace std;
using namespace dev;

namespace dev
{
	h256 const EmptyTrie = sha3(rlp(""));

	void OverlayDB::ResetDB()
	{
		m_levelDB.ResetDB();
	}

	bool OverlayDB::RefreshDB()
	{
		return m_levelDB.RefreshDB();
	}

	void OverlayDB::commit()
	{
	// #if DEV_GUARDED_DB
	// 		DEV_READ_GUARDED(x_this)
	// #endif
		{
			shared_lock<shared_timed_mutex> lock(x_this);
			m_levelDB.BatchInsert(m_main, m_aux);
		}
			
	// #if DEV_GUARDED_DB
	// 		DEV_WRITE_GUARDED(x_this)
	// #endif
		{
			unique_lock<shared_timed_mutex> lock(x_this);
			m_aux.clear();
			m_main.clear();
		}
	}

	bytes OverlayDB::lookupAux(h256 const& _h) const
	{
		bytes ret = MemoryDB::lookupAux(_h);
		if (!ret.empty())
			return ret;

		bytes b = _h.asBytes();
		b.push_back(255);	// for aux

		return asBytes(m_levelDB.Lookup(bytesConstRef(&b)));
	}

	void OverlayDB::rollback()
	{
	// #if DEV_GUARDED_DB
		// WriteGuard l(x_this);
		unique_lock<shared_timed_mutex> lock(x_this);
	// #endif
		m_main.clear();
	}

	std::string OverlayDB::lookup(h256 const& _h) const
	{
		std::string ret = MemoryDB::lookup(_h);
	
		if (ret.empty())
			ret = m_levelDB.Lookup(_h);
	
		return ret;
	}

	bool OverlayDB::exists(h256 const& _h) const
	{
		if (MemoryDB::exists(_h))
			return true;

		return m_levelDB.Exists(_h);
	}

	void OverlayDB::kill(h256 const& _h)
	{
		MemoryDB::kill(_h);
	}
}