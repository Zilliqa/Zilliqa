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
/** @file Miner.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2015
 */

#pragma once

#include <thread>
#include <list>
#include <string>
#include <sstream>
#include <boost/timer.hpp>

#include "Common.h"
#include "FixedHash.h"

#define MINER_WAIT_STATE_WORK	 1


#define DAG_LOAD_MODE_PARALLEL	 0
#define DAG_LOAD_MODE_SEQUENTIAL 1
#define DAG_LOAD_MODE_SINGLE	 2

namespace dev
{

namespace eth
{

enum class MinerType
{
	Mixed,
	CL,
	CUDA
};

enum class HwMonitorInfoType
{
	UNKNOWN,
	NVIDIA,
	AMD
};

enum class HwMonitorIndexSource
{
	UNKNOWN,
	OPENCL,
	CUDA
};

struct HwMonitorInfo
{
	HwMonitorInfoType deviceType = HwMonitorInfoType::UNKNOWN;
	HwMonitorIndexSource indexSource = HwMonitorIndexSource::UNKNOWN;
	int deviceIndex = -1;

};

struct HwMonitor
{
	int tempC = 0;
	int fanP = 0;
	double powerW = 0;
};

inline std::ostream& operator<<(std::ostream& os, HwMonitor _hw)
{
	os << _hw.tempC << "C " << _hw.fanP << "%";
	if(_hw.powerW)
		os << ' ' << std::fixed << std::setprecision(0) << _hw.powerW << "W";
	return os;
}


/// Pause mining
typedef enum
{
	MINING_NOT_PAUSED              = 0x00000000,
	MINING_PAUSED_WAIT_FOR_T_START = 0x00000001,
	MINING_PAUSED_API              = 0x00000002
	// MINING_PAUSED_USER             = 0x00000004,
	// MINING_PAUSED_ERROR            = 0x00000008
} MinigPauseReason;

struct MiningPause
{
	std::atomic<uint64_t> m_mining_paused_flag = {MinigPauseReason::MINING_NOT_PAUSED};

	void set_mining_paused(MinigPauseReason pause_reason)
	{
		m_mining_paused_flag.fetch_or(pause_reason, std::memory_order_seq_cst);
	}

	void clear_mining_paused(MinigPauseReason pause_reason)
	{
		m_mining_paused_flag.fetch_and(~pause_reason, std::memory_order_seq_cst);
	}

	MinigPauseReason get_mining_paused()
	{
		return (MinigPauseReason) m_mining_paused_flag.load(std::memory_order_relaxed);
	}

	bool is_mining_paused()
	{
		return (m_mining_paused_flag.load(std::memory_order_relaxed) != MinigPauseReason::MINING_NOT_PAUSED);
	}
};

class SolutionStats {
public:
	void accepted() { accepts++;  }
	void rejected() { rejects++;  }
	void failed()   { failures++; }

	void acceptedStale() { acceptedStales++; }

	void reset() { accepts = rejects = failures = acceptedStales = 0; }

	unsigned getAccepts()			{ return accepts; }
	unsigned getRejects()			{ return rejects; }
	unsigned getFailures()			{ return failures; }
	unsigned getAcceptedStales()	{ return acceptedStales; }

private:
	unsigned accepts  = 0;
	unsigned rejects  = 0;
	unsigned failures = 0; 

	unsigned acceptedStales = 0;
};

inline std::ostream& operator<<(std::ostream& os, SolutionStats s)
{
	os << "[A" << s.getAccepts();
	if (s.getAcceptedStales())
		os << "+" << s.getAcceptedStales();
	if (s.getRejects())
		os << ":R" << s.getRejects();
	if (s.getFailures())
		os << ":F" << s.getFailures();
	return os << "]";
}

struct WorkPackage
{
    WorkPackage() = default;
    explicit operator bool() const { return header != h256(); }

    h256 boundary;
    h256 header = h256{1u};  ///< When h256() means "pause until notified a new work package is available".
    h256 job;
    uint64_t blockNumber = -1;

    uint64_t startNonce = 0;
    int exSizeBits = -1;
    int job_len = 8;
};

struct Solution
{
	uint64_t nonce;
	h256 mixHash;
	//WorkPackage work;
	bool stale;
};

/**
 * @brief A miner - a member and adoptee of the Farm.
 * @warning Not threadsafe. It is assumed Farm will synchronise calls to/from this class.
 */
#define LOG2_MAX_MINERS 5u
#define MAX_MINERS (1u << LOG2_MAX_MINERS)

class Miner
{
public:
    Miner() = default;
	Miner(size_t _index) : m_index(_index) {}
	virtual ~Miner() = default;

	uint64_t hashCount() const { return m_hashCount.load(std::memory_order_relaxed); }

	void resetHashCount() { m_hashCount.store(0, std::memory_order_relaxed); }

	unsigned Index() { return m_index; };
	HwMonitorInfo hwmonInfo() { return m_hwmoninfo; }

	void set_mining_paused(MinigPauseReason pause_reason)
	{
		m_mining_paused.set_mining_paused(pause_reason);
	}

	void clear_mining_paused(MinigPauseReason pause_reason)
	{
		m_mining_paused.clear_mining_paused(pause_reason);
	}

	bool is_mining_paused()
	{
		return m_mining_paused.is_mining_paused();
	}

	virtual bool mine(const WorkPackage &w, Solution &solution) = 0;
	std::string getLog() const { return s_ssLog.str(); }

protected:
	void addHashCount(uint64_t _n) { m_hashCount.fetch_add(_n, std::memory_order_relaxed); }

	static unsigned s_dagLoadMode;
	static unsigned s_dagLoadIndex;
	static unsigned s_dagCreateDevice;
	static uint8_t* s_dagInHostMemory;
	static bool s_exit;
	static bool s_noeval;

	const size_t m_index = 0;
	std::chrono::high_resolution_clock::time_point workSwitchStart;
	HwMonitorInfo m_hwmoninfo;
	static std::stringstream s_ssLog;
	static std::stringstream s_ssNote;
	static std::stringstream s_ssWarn;
	WorkPackage m_currentWP;

private:
	std::atomic<uint64_t> m_hashCount = {0};
	MiningPause m_mining_paused;
};

using MinerPtr = std::unique_ptr<Miner>;

}
}
