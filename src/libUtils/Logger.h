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

#ifndef __LOGGER_H__
#define __LOGGER_H__

#include "g3log/g3log.hpp"
#include "g3log/loglevels.hpp"
#include <boost/multiprecision/cpp_int.hpp>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#define LIMIT(s, len)                                                          \
    std::setw(len) << std::setfill(' ') << std::left                           \
                   << std::string(s).substr(0, len)
#define PAD(n, len) std::setw(len) << std::setfill(' ') << std::right << n

/// Utility logging class for outputting messages to stdout or file.
class Logger
{
private:
    std::mutex m;
    bool m_logToFile;
    std::streampos m_maxFileSize;

    Logger(const char* prefix, bool log_to_file, std::streampos max_file_size);
    ~Logger();

    void checkLog();
    void newLog();

    std::string m_fileNamePrefix;
    std::string m_fileName;
    std::ofstream m_logFile;
    unsigned int m_seqNum;
    bool m_bRefactor;

public:
    /// Limits the number of bytes of a payload to display.
    static const size_t MAX_BYTES_TO_DISPLAY = 100;

    /// Limits the number of characters of the current function to display.
    static const size_t MAX_FUNCNAME_LEN = 30;

    /// Limits the number of digits of the current thread ID to display.
    static const size_t TID_LEN = 5;

    /// Limits the number of digits of the current time to display.
    static const size_t TIME_LEN = 5;

    /// Limits the output file size before rolling over to new output file.
    static const std::streampos MAX_FILE_SIZE;

    /// Returns the singleton instance for the main Logger.
    static Logger& GetLogger(const char* fname_prefix, bool log_to_file,
                             std::streampos max_file_size = MAX_FILE_SIZE);

    /// Returns the singleton instance for the state/reporting Logger.
    static Logger& GetStateLogger(const char* fname_prefix, bool log_to_file,
                                  std::streampos max_file_size = MAX_FILE_SIZE);

    /// Returns the singleton instance for the epoch info Logger.
    static Logger&
    GetEpochInfoLogger(const char* fname_prefix, bool log_to_file,
                       std::streampos max_file_size = MAX_FILE_SIZE);

    /// Outputs the specified message and function name to the state/reporting log.
    void LogState(const char* msg, const char* function);

    /// Outputs the specified message and function name to the main log.
    void LogGeneral(LEVELS level, const char* msg, const char* function);

    /// Outputs the specified message, function name, and block number to the main log.
    void LogEpoch(LEVELS level, const char* msg, const char* epoch,
                  const char* function);

    /// Outputs the specified message, function name, and payload to the main log.
    void LogMessageAndPayload(const char* msg,
                              const std::vector<unsigned char>& payload,
                              size_t max_bytes_to_display,
                              const char* function);
    /// Outputs the specified message and function name to the epoch info log.
    void LogEpochInfo(const char* msg, const char* function,
                      const char* blockNum);

    void LogPayload(LEVELS level, const char* msg,
                    const std::vector<unsigned char>& payload,
                    size_t max_bytes_to_display, const char* function);

    /// Setup the display debug level
    ///     INFO: display all message
    ///     WARNING: display warning and fatal message
    ///     FATAL: display fatal message only
    void DisplayLevelAbove(LEVELS level = INFO);

    /// Enable the log level
    void EnableLevel(LEVELS level);

    /// Disable the log level
    void DisableLevel(LEVELS level);

    /// See if we need to use g3log or not
    bool IsG3Log() { return (m_logToFile && m_bRefactor); };

    /// Get current process id
    static pid_t GetPid();

    /// Calculate payload string according to payload vector & length
    static void GetPayloadS(const std::vector<unsigned char>& payload,
                            size_t max_bytes_to_display,
                            std::unique_ptr<char[]>& res);
};

/// Utility class for automatically logging function or code block exit.
class ScopeMarker
{
    std::string m_function;

public:
    /// Constructor.
    ScopeMarker(const char* function);

    /// Destructor.
    ~ScopeMarker();
};

#define INIT_FILE_LOGGER(fname_prefix) Logger::GetLogger(fname_prefix, true)
#define INIT_STDOUT_LOGGER() Logger::GetLogger(NULL, false)
#define INIT_STATE_LOGGER(fname_prefix)                                        \
    Logger::GetStateLogger(fname_prefix, true)
#define INIT_EPOCHINFO_LOGGER(fname_prefix)                                    \
    Logger::GetEpochInfoLogger(fname_prefix, true)
#define LOG_MARKER() ScopeMarker marker(__FUNCTION__)
#define LOG_STATE(msg)                                                         \
    {                                                                          \
        std::ostringstream oss;                                                \
        oss << msg;                                                            \
        Logger::GetStateLogger(NULL, true)                                     \
            .LogState(oss.str().c_str(), __FUNCTION__);                        \
    }
#define LOG_GENERAL(level, msg)                                                \
    {                                                                          \
        if (Logger::GetLogger(NULL, true).IsG3Log())                           \
        {                                                                      \
            std::time_t curTime = std::chrono::system_clock::to_time_t(        \
                std::chrono::system_clock::now());                             \
            LOG(level) << "[TID " << PAD(Logger::GetPid(), Logger::TID_LEN)    \
                       << "][" << std::put_time(gmtime(&curTime), "%H:%M:%S")  \
                       << "]["                                                 \
                       << LIMIT(__FUNCTION__, Logger::MAX_FUNCNAME_LEN)        \
                       << "] " << msg;                                         \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            std::ostringstream oss;                                            \
            oss << msg;                                                        \
            Logger::GetLogger(NULL, true)                                      \
                .LogGeneral(level, oss.str().c_str(), __FUNCTION__);           \
        }                                                                      \
    }
#define LOG_EPOCH(level, epoch, msg)                                           \
    {                                                                          \
        if (Logger::GetLogger(NULL, true).IsG3Log())                           \
        {                                                                      \
            std::time_t curTime = std::chrono::system_clock::to_time_t(        \
                std::chrono::system_clock::now());                             \
            LOG(level) << "[TID " << PAD(Logger::GetPid(), Logger::TID_LEN)    \
                       << "][" << std::put_time(gmtime(&curTime), "%H:%M:%S")  \
                       << "]["                                                 \
                       << LIMIT(__FUNCTION__, Logger::MAX_FUNCNAME_LEN) << "]" \
                       << "[Epoch " << epoch << "] " << msg;                   \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            std::ostringstream oss;                                            \
            oss << msg;                                                        \
            Logger::GetLogger(NULL, true)                                      \
                .LogEpoch(level, epoch, oss.str().c_str(), __FUNCTION__);      \
        }                                                                      \
    }
#define LOG_PAYLOAD(level, msg, payload, max_bytes_to_display)                 \
    {                                                                          \
        if (Logger::GetLogger(NULL, true).IsG3Log())                           \
        {                                                                      \
            std::time_t curTime = std::chrono::system_clock::to_time_t(        \
                std::chrono::system_clock::now());                             \
            std::unique_ptr<char[]> payload_string;                            \
            Logger::GetPayloadS(payload, max_bytes_to_display,                 \
                                payload_string);                               \
            if ((payload).size() > max_bytes_to_display)                       \
            {                                                                  \
                LOG(level) << "[TID "                                          \
                           << PAD(Logger::GetPid(), Logger::TID_LEN) << "]["   \
                           << std::put_time(gmtime(&curTime), "%H:%M:%S")      \
                           << "]["                                             \
                           << LIMIT(__FUNCTION__, Logger::MAX_FUNCNAME_LEN)    \
                           << "] " << msg << " (Len=" << (payload).size()      \
                           << "): " << payload_string.get() << "...";          \
            }                                                                  \
            else                                                               \
            {                                                                  \
                LOG(level) << "[TID "                                          \
                           << PAD(Logger::GetPid(), Logger::TID_LEN) << "]["   \
                           << std::put_time(gmtime(&curTime), "%H:%M:%S")      \
                           << "]["                                             \
                           << LIMIT(__FUNCTION__, Logger::MAX_FUNCNAME_LEN)    \
                           << "] " << msg << " (Len=" << (payload).size()      \
                           << "): " << payload_string.get();                   \
            }                                                                  \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            std::ostringstream oss;                                            \
            oss << msg;                                                        \
            Logger::GetLogger(NULL, true)                                      \
                .LogPayload(level, oss.str().c_str(), payload,                 \
                            max_bytes_to_display, __FUNCTION__);               \
        }                                                                      \
    }
#define LOG_DISPLAY_LEVEL_ABOVE(level)                                         \
    {                                                                          \
        Logger::GetLogger(NULL, true).DisplayLevelAbove(level);                \
    }
#define LOG_ENABLE_LEVEL(level)                                                \
    {                                                                          \
        Logger::GetLogger(NULL, true).EnableLevel(level);                      \
    }
#define LOG_DISABLE_LEVEL(level)                                               \
    {                                                                          \
        Logger::GetLogger(NULL, true).DisableLevel(level);                     \
    }
#define LOG_EPOCHINFO(blockNum, msg)                                           \
    {                                                                          \
        std::ostringstream oss;                                                \
        oss << msg;                                                            \
        Logger::GetEpochInfoLogger(NULL, true)                                 \
            .LogEpochInfo(oss.str().c_str(), __FUNCTION__, blockNum);          \
    }
#endif // __LOGGER_H__
