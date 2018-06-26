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
    this->m_logToFile = log_to_file;
    this->m_maxFileSize = max_file_size;

    if (log_to_file)
    {
        m_fileNamePrefix = prefix ? prefix : "common";
        m_seqNum = 0;
        newLog();
    }
}

Logger::~Logger() { m_logFile.close(); }

void Logger::checkLog()
{
    std::ifstream in(m_fileName.c_str(),
                     std::ifstream::ate | std::ifstream::binary);

    if (in.tellg() >= m_maxFileSize)
    {
        m_logFile.close();
        newLog();
    }
}

void Logger::newLog()
{
    m_seqNum++;
    m_bRefactor = (m_fileNamePrefix == "zilliqa");

    // Filename = m_fileNamePrefix + 5-digit sequence number + "-log.txt"
    char buf[16] = {0};
    snprintf(buf, sizeof(buf), (m_bRefactor ? "-%05d-log" : "-%05d-log.txt"),
             m_seqNum);
    m_fileName = m_fileNamePrefix + buf;

    if (m_bRefactor)
    {
        logworker = LogWorker::createLogWorker();
        auto sinkHandle = logworker->addSink(
            std::make_unique<FileSink>(m_fileName.c_str(), "./", ""),
            &FileSink::fileWrite);
        sinkHandle->call(&g3::FileSink::overrideLogDetails, &MyCustomFormatting)
            .wait();
        sinkHandle->call(&g3::FileSink::overrideLogHeader, "").wait();
        initializeLogging(logworker.get());
    }
    else
    {
        m_logFile.open(m_fileName.c_str(), ios_base::app);
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

Logger& Logger::GetEpochInfoLogger(const char* fname_prefix, bool log_to_file,
                                   streampos max_file_size)
{
    static Logger logger(fname_prefix, log_to_file, max_file_size);
    return logger;
}

void Logger::LogState(const char* msg, const char*)
{
    lock_guard<mutex> guard(m);

    if (m_logToFile)
    {
        checkLog();
        m_logFile << msg << endl << flush;
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

    if (m_logToFile)
    {
        checkLog();
        m_logFile << "[TID " << PAD(GetPid(), TID_LEN) << "]["
                  << PAD(put_time(gmtime(&curTime), "%H:%M:%S"), TIME_LEN)
                  << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg
                  << endl
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

void Logger::LogEpoch([[gnu::unused]] LEVELS level, const char* msg,
                      const char* epoch, const char* function)
{
    std::time_t curTime = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());

    lock_guard<mutex> guard(m);

    if (m_logToFile)
    {
        checkLog();
        m_logFile << "[TID " << PAD(GetPid(), TID_LEN) << "]["
                  << PAD(put_time(gmtime(&curTime), "%H:%M:%S"), TIME_LEN)
                  << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "]"
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

void Logger::LogPayload([[gnu::unused]] LEVELS level, const char* msg,
                        const std::vector<unsigned char>& payload,
                        size_t max_bytes_to_display, const char* function)
{
    std::unique_ptr<char[]> payload_string;
    GetPayloadS(payload, max_bytes_to_display, payload_string);
    std::time_t curTime = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());

    lock_guard<mutex> guard(m);

    if (m_logToFile)
    {
        checkLog();

        if (payload.size() > max_bytes_to_display)
        {
            m_logFile << "[TID " << PAD(GetPid(), TID_LEN) << "]["
                      << PAD(put_time(gmtime(&curTime), "%H:%M:%S"), TIME_LEN)
                      << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "] "
                      << msg << " (Len=" << payload.size()
                      << "): " << payload_string.get() << "..." << endl
                      << flush;
        }
        else
        {
            m_logFile << "[TID " << PAD(GetPid(), TID_LEN) << "]["
                      << PAD(put_time(gmtime(&curTime), "%H:%M:%S"), TIME_LEN)
                      << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "] "
                      << msg << " (Len=" << payload.size()
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

void Logger::LogEpochInfo(const char* msg, const char* function,
                          const char* epoch)
{
    pid_t tid = getCurrentPid();

    std::time_t curTime = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());

    lock_guard<mutex> guard(m);

    if (m_logToFile)
    {
        checkLog();
        m_logFile << "[TID " << PAD(tid, TID_LEN) << "]["
                  << PAD(put_time(gmtime(&curTime), "%H:%M:%S"), TIME_LEN)
                  << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "]"
                  << "[Epoch " << epoch << "] " << msg << endl
                  << flush;
    }
    else
    {
        cout << "[TID " << PAD(tid, TID_LEN) << "]["
             << PAD(put_time(gmtime(&curTime), "%H:%M:%S"), TIME_LEN) << "]["
             << LIMIT(function, MAX_FUNCNAME_LEN) << "]"
             << "[Epoch " << epoch << "] " << msg << endl
             << flush;
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
    : m_function(function)
{
    Logger& logger = Logger::GetLogger(NULL, true);
    logger.LogGeneral(INFO, "BEGIN", this->m_function.c_str());
}

ScopeMarker::~ScopeMarker()
{
    Logger& logger = Logger::GetLogger(NULL, true);
    logger.LogGeneral(INFO, "END", m_function.c_str());
}
