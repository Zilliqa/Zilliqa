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

#include "g3log/logworker.hpp"
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

using namespace std;
using namespace g3;
unique_ptr<LogWorker> logworker;
unique_ptr<SinkHandle<FileSink>> sinkHandle;

string MyCustomFormatting(const LogMessage& msg)
{
    string res = string("[") + msg.level();

    for (unsigned int i = msg.level().size(); i < WARNING.text.size(); ++i)
    {
        res += " ";
    }

    res += "]";
    return res;
}

namespace
{
    /// helper function to get tid with better cross-platform support
    inline pid_t getCurrentPid()
    {
#if defined(__linux__)
        return syscall(SYS_gettid);
#elif defined(__APPLE__) && defined(__MACH__)
        uint64_t tid64;
        pthread_threadid_np(NULL, &tid64);
        return static_cast<pid_t>(tid64);
#else
#error // not implemented in this platform
        return 0;
#endif
    }
};

const streampos Logger::MAX_FILE_SIZE = 1024 * 1024 * 100; // 100MB per log file

Logger::Logger(const char* prefix, bool log_to_file, streampos max_file_size)
{
    this->log_to_file = log_to_file;
    this->max_file_size = max_file_size;

    if (log_to_file)
    {
        fname_prefix = prefix ? prefix : "common";
        seqnum = 0;
        newLog();
    }
}

Logger::~Logger() { logfile.close(); }

void Logger::CheckLog()
{
    if (IsG3Log())
    {
        future<string> fileName = sinkHandle->call(&g3::FileSink::fileName);
        std::ifstream in(fileName.get(),
                         std::ifstream::ate | std::ifstream::binary);

        if (in.tellg() >= max_file_size)
        {
            g3::internal::shutDownLogging();
            newLog();
        }

        return;
    }

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

    bRefactor = (fname.substr(0, 7) == "zilliqa");

    if (bRefactor)
    {
        logworker = LogWorker::createLogWorker();
        sinkHandle = logworker->addSink(
            std::make_unique<FileSink>(fname.c_str(), "./"),
            &FileSink::fileWrite);
        sinkHandle->call(&g3::FileSink::overrideLogDetails, &MyCustomFormatting)
            .wait();
        sinkHandle->call(&g3::FileSink::overrideLogHeader, "").wait();
        initializeLogging(logworker.get());
    }
}

Logger& Logger::GetLogger(const char* fname_prefix, bool log_to_file,
                          streampos max_file_size)
{
    static Logger logger(fname_prefix, log_to_file, max_file_size);
    return logger;
}

Logger& Logger::GetStateLogger(const char* fname_prefix, bool log_to_file,
                               streampos max_file_size)
{
    static Logger logger(fname_prefix, log_to_file, max_file_size);
    return logger;
}

void Logger::LogState(const char* msg, const char*)
{
    lock_guard<mutex> guard(m);

    if (log_to_file)
    {
        CheckLog();
        logfile << msg << endl << flush;
    }
    else
    {
        cout << msg << endl << flush;
    }
}

void Logger::LogGeneral(LEVELS level, const char* msg, const char* function)
{
    std::time_t curTime = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());

    if (IsG3Log())
    {
        LOG(level) << "[TID " << PAD(GetPid(), TID_LEN) << "]["
                   << put_time(gmtime(&curTime), "%H:%M:%S") << "]["
                   << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg;
        return;
    }

    lock_guard<mutex> guard(m);

    if (log_to_file)
    {
        CheckLog();
        logfile << "[TID " << PAD(GetPid(), TID_LEN) << "]["
                << PAD(put_time(gmtime(&curTime), "%H:%M:%S"), TIME_LEN) << "]["
                << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg << endl
                << flush;
    }
    else
    {
        cout << "[TID " << PAD(GetPid(), TID_LEN) << "]["
             << PAD(put_time(gmtime(&curTime), "%H:%M:%S"), TIME_LEN) << "]["
             << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg << endl
             << flush;
    }
}

void Logger::LogEpoch(LEVELS level, const char* msg, const char* epoch,
                      const char* function)
{
    std::time_t curTime = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());

    lock_guard<mutex> guard(m);

    if (log_to_file)
    {
        CheckLog();
        logfile << "[TID " << PAD(GetPid(), TID_LEN) << "]["
                << PAD(put_time(gmtime(&curTime), "%H:%M:%S"), TIME_LEN) << "]["
                << LIMIT(function, MAX_FUNCNAME_LEN) << "]"
                << "[Epoch " << epoch << "] " << msg << endl
                << flush;
    }
    else
    {
        cout << "[TID " << PAD(GetPid(), TID_LEN) << "]["
             << PAD(put_time(gmtime(&curTime), "%H:%M:%S"), TIME_LEN) << "]["
             << LIMIT(function, MAX_FUNCNAME_LEN) << "]"
             << "[Epoch " << epoch << "] " << msg << endl
             << flush;
    }
}

void Logger::LogPayload(LEVELS level, const char* msg,
                        const std::vector<unsigned char>& payload,
                        size_t max_bytes_to_display, const char* function)
{
    std::unique_ptr<char[]> payload_string;
    GetPayloadS(payload, max_bytes_to_display, payload_string);
    std::time_t curTime = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());

    lock_guard<mutex> guard(m);

    if (log_to_file)
    {
        CheckLog();

        if (payload.size() > max_bytes_to_display)
        {
            logfile << "[TID " << PAD(GetPid(), TID_LEN) << "]["
                    << PAD(put_time(gmtime(&curTime), "%H:%M:%S"), TIME_LEN)
                    << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg
                    << " (Len=" << payload.size()
                    << "): " << payload_string.get() << "..." << endl
                    << flush;
        }
        else
        {
            logfile << "[TID " << PAD(GetPid(), TID_LEN) << "]["
                    << PAD(put_time(gmtime(&curTime), "%H:%M:%S"), TIME_LEN)
                    << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg
                    << " (Len=" << payload.size()
                    << "): " << payload_string.get() << endl
                    << flush;
        }
    }
    else
    {
        if (payload.size() > max_bytes_to_display)
        {
            cout << "[TID " << PAD(GetPid(), TID_LEN) << "]["
                 << PAD(put_time(gmtime(&curTime), "%H:%M:%S"), TIME_LEN)
                 << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg
                 << " (Len=" << payload.size() << "): " << payload_string.get()
                 << "..." << endl
                 << flush;
        }
        else
        {
            cout << "[TID " << PAD(GetPid(), TID_LEN) << "]["
                 << PAD(put_time(gmtime(&curTime), "%H:%M:%S"), TIME_LEN)
                 << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg
                 << " (Len=" << payload.size() << "): " << payload_string.get()
                 << endl
                 << flush;
        }
    }
}

void Logger::DisplayLevelAbove(LEVELS level)
{
    if (level != INFO && level != WARNING && level != FATAL)
        return;

    g3::log_levels::setHighest(level);
}

void Logger::EnableLevel(LEVELS level) { g3::log_levels::enable(level); }

void Logger::DisableLevel(LEVELS level) { g3::log_levels::disable(level); }

pid_t Logger::GetPid() { return getCurrentPid(); }

void Logger::GetPayloadS(const std::vector<unsigned char>& payload,
                         size_t max_bytes_to_display,
                         std::unique_ptr<char[]>& res)
{
    static const char* hex_table = "0123456789ABCDEF";
    size_t payload_string_len = (payload.size() * 2) + 1;

    if (payload.size() > max_bytes_to_display)
    {
        payload_string_len = (max_bytes_to_display * 2) + 1;
    }

    res = make_unique<char[]>(payload_string_len);

    for (unsigned int payload_idx = 0, payload_string_idx = 0;
         (payload_idx < payload.size())
         && ((payload_string_idx + 2) < payload_string_len);
         payload_idx++)
    {
        res.get()[payload_string_idx++]
            = hex_table[(payload.at(payload_idx) >> 4) & 0xF];
        res.get()[payload_string_idx++]
            = hex_table[payload.at(payload_idx) & 0xF];
    }

    res.get()[payload_string_len - 1] = '\0';
}

ScopeMarker::ScopeMarker(const char* function)
    : function(function)
{
    Logger& logger = Logger::GetLogger(NULL, true);
    logger.LogGeneral(INFO, "BEGIN", this->function.c_str());
}

ScopeMarker::~ScopeMarker()
{
    Logger& logger = Logger::GetLogger(NULL, true);
    logger.LogGeneral(INFO, "END", function.c_str());
}
