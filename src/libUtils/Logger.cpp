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

#include "Logger.h"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>

using namespace std;

#define LIMIT(s, len) setw(len) << setfill(' ') << left << string(s).substr(0, len)
#define PAD(n, len) setw(len) << setfill(' ') << right << n

const streampos Logger::MAX_FILE_SIZE = 1024 * 1024 * 100; // 100MB per log file

Logger::Logger(const char * prefix, bool log_to_file, streampos max_file_size)
{
    this->log_to_file = log_to_file;
    this->max_file_size = max_file_size;

    if (log_to_file)
    {
        if (prefix == NULL)
        {
            fname_prefix = "common";
        }
        else
        {
            fname_prefix = prefix;
        }

        seqnum = 0;
        newLog();
    }
}

Logger::~Logger()
{
    logfile.close();
}

void Logger::checkLog()
{
    std::ifstream in(fname.c_str(), std::ifstream::ate | std::ifstream::binary);

    if (in.tellg() >= max_file_size)
    {
        logfile.close();
        newLog();
    }
}

void Logger::newLog()
{
    seqnum++;

    // Filename = fname_prefix + 5-digit sequence number + "-log.txt"
    char buf[16] = {0};
    snprintf(buf, sizeof(buf), "-%05d-log.txt", seqnum);
    fname = fname_prefix + buf;
    logfile.open(fname.c_str(), ios_base::app);
}

Logger & Logger::GetLogger(const char * fname_prefix, bool log_to_file, streampos max_file_size)
{
    static Logger logger(fname_prefix, log_to_file, max_file_size);
    return logger;
}

Logger & Logger::GetStateLogger(const char * fname_prefix, bool log_to_file, streampos max_file_size)
{
    static Logger logger(fname_prefix, log_to_file, max_file_size);
    return logger;
}

void Logger::LogMessage(const char * msg, const char * function)
{
    std::thread::id tid = std::this_thread::get_id();

    auto clockNow = std::chrono::system_clock::now();
    std::time_t curTime = std::chrono::system_clock::to_time_t(clockNow);
    auto gmtTime = gmtime(&curTime);

    lock_guard<mutex> guard(m);

    if (log_to_file)
    {
        checkLog();
        logfile << "[TID " << PAD(tid, TID_LEN) << "][" << PAD(put_time(gmtTime, "%H:%M:%S"), TIME_LEN) 
                << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg << endl << flush;
    }
    else
    {
        cout << "[TID " << PAD(tid, TID_LEN) << "][" << PAD(put_time(gmtTime, "%H:%M:%S"), TIME_LEN) << "][" 
             << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg << endl << flush;
    }
}

void Logger::LogMessage(const char * msg, const char * function, const char * epoch)
{
    std::thread::id tid = std::this_thread::get_id();

    auto clockNow = std::chrono::system_clock::now();
    std::time_t curTime = std::chrono::system_clock::to_time_t(clockNow);
    auto gmtTime = gmtime(&curTime);

    lock_guard<mutex> guard(m);

    if (log_to_file)
    {
        checkLog();
        logfile << "[TID " << PAD(tid, TID_LEN) << "][" << PAD(put_time(gmtTime, "%H:%M:%S"), TIME_LEN)
                << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "]" << "[Epoch " << epoch << "] " 
                << msg << endl << flush;
    }
    else
    {
        cout << "[TID " << PAD(tid, TID_LEN) << "][" << PAD(put_time(gmtTime, "%H:%M:%S"), TIME_LEN)
             << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "]" << "[Epoch " << epoch << "] " 
             << msg << endl << flush;
    }
}

void Logger::LogState(const char * msg, const char * function)
{
    lock_guard<mutex> guard(m);

    if (log_to_file)
    {
        checkLog();
        logfile << msg << endl << flush;
    }
    else
    {
        cout << msg << endl << flush;
    }
}

void Logger::LogMessageAndPayload(const char * msg, const vector<unsigned char> & payload, size_t max_bytes_to_display, const char * function)
{
    std::thread::id tid = std::this_thread::get_id();

    static const char * hex_table = "0123456789ABCDEF";

    size_t payload_string_len = (payload.size() * 2) + 1;
    if (payload.size() > max_bytes_to_display)
    {
        payload_string_len = (max_bytes_to_display * 2) + 1;
    }

    unique_ptr<char[]> payload_string = make_unique<char[]>(payload_string_len);
    for (unsigned int payload_idx = 0, payload_string_idx = 0; (payload_idx < payload.size()) && ((payload_string_idx + 2) < payload_string_len); payload_idx++)
    {
        payload_string.get()[payload_string_idx++] = hex_table[(payload.at(payload_idx) >> 4) & 0xF];
        payload_string.get()[payload_string_idx++] = hex_table[payload.at(payload_idx) & 0xF];
    }
    payload_string.get()[payload_string_len-1] = '\0';

    auto clockNow = std::chrono::system_clock::now();
    std::time_t curTime = std::chrono::system_clock::to_time_t(clockNow); 
    auto gmtTime = gmtime(&curTime);   

    lock_guard<mutex> guard(m);

    if (log_to_file)
    {
        checkLog();

        if (payload.size() > max_bytes_to_display)
        {
            logfile << "[TID " << PAD(tid, TID_LEN) << "][" << PAD(put_time(gmtTime, "%H:%M:%S"), TIME_LEN)
                    << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg 
                    << " (Len=" << payload.size() << "): " << payload_string.get() << "..." 
                    << endl << flush;
        }
        else
        {
            logfile << "[TID " << PAD(tid, TID_LEN) << "][" << PAD(put_time(gmtTime, "%H:%M:%S"), TIME_LEN)
                    << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg 
                    << " (Len=" << payload.size() << "): " << payload_string.get() << endl << flush;
        }
    }
    else
    {
        if (payload.size() > max_bytes_to_display)
        {
            cout << "[TID " << PAD(tid, TID_LEN) << "][" << PAD(put_time(gmtTime, "%H:%M:%S"), TIME_LEN)
                 << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg 
                 << " (Len=" << payload.size() << "): " << payload_string.get() << "..." 
                 << endl << flush;
        }
        else
        {
            cout << "[TID " << PAD(tid, TID_LEN) << "][" << PAD(put_time(gmtTime, "%H:%M:%S"), TIME_LEN)
                 << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg << " (Len=" 
                 << payload.size() << "): " << payload_string.get() << endl << flush;
        }
    }
}

ScopeMarker::ScopeMarker(const char * function) : function(function)
{
    Logger & logger = Logger::GetLogger(NULL, true);
    logger.LogMessage("BEGIN", this->function.c_str());
}

ScopeMarker::~ScopeMarker()
{
    Logger & logger = Logger::GetLogger(NULL, true);
    logger.LogMessage("END", function.c_str());
}
