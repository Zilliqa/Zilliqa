/** ==========================================================================
* 2011 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
 * 
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================*/
#pragma once

#include <iostream>
#include <sstream>
#include <fstream>
#include <ctime>
#include <cstdio>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <cassert>

#if defined(G3LOG_PERFORMANCE)
#include <g3log/g3log.hpp>
#include <g3log/logworker.hpp>
using namespace g3::internal;

#elif defined(GOOGLE_GLOG_PERFORMANCE)
#include <glog/logging.h>
#else
#error G3LOG_PERFORMANCE or GOOGLE_GLOG_PERFORMANCE was not defined
#endif

typedef std::chrono::high_resolution_clock::time_point time_point;
typedef std::chrono::duration<uint64_t,std::ratio<1, 1000> > millisecond;
typedef std::chrono::duration<uint64_t,std::ratio<1, 1000000> > microsecond;

namespace g3_test
{
enum WriteMode
{
  kAppend = 0,
  kTruncate = 1
};

const uint64_t g_loop{1};
const uint64_t g_iterations{1000000};
const char* charptrmsg = "\tmessage by char*";
const std::string strmsg{"\tmessage by string"};
float pi_f{3.1415926535897932384626433832795f};


bool writeTextToFile(const std::string& filename, const std::string& msg, const WriteMode write_mode, bool push_out = true)
{
  if(push_out)
  {
    std::cout << msg << std::flush;
  }

  std::ofstream out;
  std::ios_base::openmode mode = std::ios_base::out; // for clarity: it's really overkill since it's an ofstream
  (kTruncate == write_mode) ? mode |= std::ios_base::trunc : mode |= std::ios_base::app;
  out.open(filename.c_str(), mode);
  if (!out.is_open())
  {
    std::ostringstream ss_error;
    ss_error << "Fatal error could not open log file:[" << filename << "]";
    ss_error << "\n\t\t std::ios_base state = " << out.rdstate();
    std::cerr << ss_error.str().c_str() << std::endl << std::flush;
    return false;
  }

  out << msg;
  return true;
}

uint64_t mean(const std::vector<uint64_t> &v)
{
  uint64_t total =  std::accumulate(v.begin(), v.end(), uint64_t(0) ); // '0' is the initial value
  return total/v.size();
}




void measurePeakDuringLogWrites(const std::string& title, std::vector<uint64_t>& result);
inline void measurePeakDuringLogWrites(const std::string& title, std::vector<uint64_t>& result)
{


#if defined(G3LOG_PERFORMANCE)
  std::cout << "G3LOG (" << title << ") WORST_PEAK PERFORMANCE TEST" << std::endl;
#elif defined(GOOGLE_GLOG_PERFORMANCE)
  std::cout << "GOOGLE_GLOG (" << title << ") WORST_PEAK PERFORMANCE TEST" << std::endl;
#else
  std::cout << "ERROR no performance type chosen" << std::endl;
  assert(false);
#endif
  for(uint64_t count = 0; count < g_iterations; ++count)
  {
    auto start_time = std::chrono::high_resolution_clock::now();
    LOG(INFO) << title << " iteration #" << count << " " << charptrmsg << strmsg << " and a float: " << std::setprecision(6) << pi_f;
    auto stop_time = std::chrono::high_resolution_clock::now();
    uint64_t time_us = std::chrono::duration_cast<microsecond>(stop_time - start_time).count();
    result.push_back(time_us);
  }
}


void doLogWrites(const std::string& title);
inline void doLogWrites(const std::string& title)
{
#if defined(G3LOG_PERFORMANCE)
  std::cout << "G3LOG (" << title << ") PERFORMANCE TEST" << std::endl;
#elif defined(GOOGLE_GLOG_PERFORMANCE)
  std::cout << "GOOGLE_GLOG (" << title << ") PERFORMANCE TEST" << std::endl;
#else
  std::cout << "ERROR no performance type chosen" << std::endl;
  assert(false);
#endif
  for(uint64_t count = 0; count < g_iterations; ++count)
  {
    LOG(INFO) << title << " iteration #" << count << " " << charptrmsg << strmsg << " and a float: " << std::setprecision(6) << pi_f;
  }
}


} // end namespace
