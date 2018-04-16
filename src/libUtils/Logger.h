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

#include <boost/multiprecision/cpp_int.hpp>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

/// Utility logging class for outputting messages to stdout or file.
class Logger
{
private:
    std::mutex m;
    bool log_to_file;
    std::streampos max_file_size;

    Logger(const char* prefix, bool log_to_file, std::streampos max_file_size);
    ~Logger();

    void checkLog();
    void newLog();

    std::string fname_prefix;
    std::string fname;
    std::ofstream logfile;
    unsigned int seqnum;
#if 1//clark
    bool bPreserve;
#endif

public:
    /// Limits the number of bytes of a payload to display.
    static const size_t MAX_BYTES_TO_DISPLAY = 100;

    /// Limits the number of characters of the current function to display.
    static const size_t MAX_FUNCNAME_LEN = 20;

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

    /// Outputs the specified message and function name to the main log.
    void LogMessage(const char* msg, const char* function);

    /// Outputs the specified message, function name, and block number to the main log.
    void LogMessage(const char* msg, const char* function,
                    const char* blockNum);

    /// Outputs the specified message and function name to the state/reporting log.
    void LogState(const char* msg, const char* function);

    /// Outputs the specified message, function name, and payload to the main log.
    void LogMessageAndPayload(const char* msg,
                              const std::vector<unsigned char>& payload,
                              size_t max_bytes_to_display,
                              const char* function);
};

/// Utility class for automatically logging function or code block exit.
class ScopeMarker
{
    std::string function;

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
#define LOG_MARKER() ScopeMarker marker(__FUNCTION__)
#define LOG_MESSAGE(msg)                                                       \
    {                                                                          \
        std::ostringstream oss;                                                \
        oss << msg;                                                            \
        Logger::GetLogger(NULL, true)                                          \
            .LogMessage(oss.str().c_str(), __FUNCTION__);                      \
    }
#define LOG_MESSAGE2(blockNum, msg)                                            \
    {                                                                          \
        std::ostringstream oss;                                                \
        oss << msg;                                                            \
        Logger::GetLogger(NULL, true)                                          \
            .LogMessage(oss.str().c_str(), __FUNCTION__, blockNum);            \
    }
#define LOG_PAYLOAD(msg, payload, max_bytes_to_display)                        \
    {                                                                          \
        std::ostringstream oss;                                                \
        oss << msg;                                                            \
        Logger::GetLogger(NULL, true)                                          \
            .LogMessageAndPayload(oss.str().c_str(), payload,                  \
                                  max_bytes_to_display, __FUNCTION__);         \
    }
#define LOG_STATE(msg)                                                         \
    {                                                                          \
        std::ostringstream oss;                                                \
        oss << msg;                                                            \
        Logger::GetStateLogger(NULL, true)                                     \
            .LogState(oss.str().c_str(), __FUNCTION__);                        \
    }

#endif // __LOGGER_H__
