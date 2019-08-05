/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef ZILLIQA_SRC_LIBUTILS_LOGGER_H_
#define ZILLIQA_SRC_LIBUTILS_LOGGER_H_

#include <boost/filesystem.hpp>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include "common/BaseType.h"
#include "g3log/g3log.hpp"
#include "g3log/loglevels.hpp"
#include "g3log/logworker.hpp"
#include "libUtils/TimeUtils.h"

#define LIMIT(s, len)                              \
  std::setw(len) << std::setfill(' ') << std::left \
                 << std::string(s).substr(0, len)

#define LIMIT_RIGHT(s, len)                        \
  std::setw(len) << std::setfill(' ') << std::left \
                 << s.substr(std::max<int>((int)s.size() - len, 0))

#define PAD(n, len, ch) std::setw(len) << std::setfill(ch) << std::right << n

/// Utility logging class for outputting messages to stdout or file.
class Logger {
 private:
  std::mutex m;
  bool m_logToFile;
  std::streampos m_maxFileSize;
  std::unique_ptr<g3::LogWorker> logworker;

  Logger(const char* prefix, bool log_to_file, const char* logpath,
         std::streampos max_file_size);
  ~Logger();

  void checkLog();
  void newLog();

  std::string m_fileNamePrefix;
  std::string m_fileName;
  std::ofstream m_logFile;
  unsigned int m_seqNum;
  bool m_bRefactor{};
  std::string m_logPath;

 public:
  /// Limits the number of bytes of a payload to display.
  static const size_t MAX_BYTES_TO_DISPLAY = 30;

  /// Limits the number of characters of the current filename and line number to
  /// display.
  static const size_t MAX_FILEANDLINE_LEN = 20;

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
                           const char* logpath,
                           std::streampos max_file_size = MAX_FILE_SIZE);

  /// Returns the singleton instance for the state/reporting Logger.
  static Logger& GetStateLogger(const char* fname_prefix, bool log_to_file,
                                const char* logpath,
                                std::streampos max_file_size = MAX_FILE_SIZE);

  /// Returns the singleton instance for the epoch info Logger.
  static Logger& GetEpochInfoLogger(
      const char* fname_prefix, bool log_to_file, const char* logpath,
      std::streampos max_file_size = MAX_FILE_SIZE);

  /// Outputs the specified message and function name to the state/reporting
  /// log.
  void LogState(const char* msg);

  /// Outputs the specified message and function name to the main log.
  void LogGeneral(const LEVELS& level, const char* msg,
                  const unsigned int linenum, const char* filename,
                  const char* function);

  /// Outputs the specified message, function name, and block number to the main
  /// log.
  void LogEpoch(const LEVELS& level, const char* msg, const char* epoch,
                const unsigned int linenum, const char* filename,
                const char* function);

  /// Outputs the specified message, function name, and payload to the main log.
  void LogMessageAndPayload(const char* msg, const bytes& payload,
                            size_t max_bytes_to_display,
                            const unsigned int linenum, const char* filename,
                            const char* function);
  /// Outputs the specified message and function name to the epoch info log.
  void LogEpochInfo(const char* msg, const unsigned int linenum,
                    const char* filename, const char* function,
                    const char* epoch);

  void LogPayload(const LEVELS& level, const char* msg, const bytes& payload,
                  size_t max_bytes_to_display, const unsigned int linenum,
                  const char* filename, const char* function);

  /// Setup the display debug level
  ///     INFO: display all message
  ///     WARNING: display warning and fatal message
  ///     FATAL: display fatal message only
  void DisplayLevelAbove(const LEVELS& level = INFO);

  /// Enable the log level
  void EnableLevel(const LEVELS& level);

  /// Disable the log level
  void DisableLevel(const LEVELS& level);

  /// See if we need to use g3log or not
  bool IsG3Log() { return (m_logToFile && m_bRefactor); };

  /// Get current process id
  static pid_t GetPid();

  /// Calculate payload string according to payload vector & length
  static void GetPayloadS(const bytes& payload, size_t max_bytes_to_display,
                          std::unique_ptr<char[]>& res);
};

/// Utility class for automatically logging function or code block exit.
class ScopeMarker {
  unsigned int m_linenum;
  std::string m_filename;
  std::string m_function;

 public:
  /// Constructor.
  ScopeMarker(const unsigned int linenum, const char* filename,
              const char* function);

  /// Destructor.
  ~ScopeMarker();
};

#define INIT_FILE_LOGGER(fname_prefix, logpath) \
  Logger::GetLogger(fname_prefix, true, logpath)
#define INIT_STDOUT_LOGGER()     \
  Logger::GetLogger(NULL, false, \
                    boost::filesystem::absolute("./").string().c_str())
#define INIT_STATE_LOGGER(fname_prefix, logpath) \
  Logger::GetStateLogger(fname_prefix, true, logpath)
#define INIT_EPOCHINFO_LOGGER(fname_prefix, logpath) \
  Logger::GetEpochInfoLogger(fname_prefix, true, logpath)
#define LOG_MARKER() ScopeMarker marker(__LINE__, __FILE__, __FUNCTION__)
#define LOG_STATE(msg)                                                         \
  {                                                                            \
    std::ostringstream oss;                                                    \
    auto cur = std::chrono::system_clock::now();                               \
    auto cur_time_t = std::chrono::system_clock::to_time_t(cur);               \
    oss << "[ " << std::put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")          \
        << PAD(get_ms(cur), 3, '0') << " ]" << msg;                            \
    Logger::GetStateLogger(NULL, true,                                         \
                           boost::filesystem::absolute("./").string().c_str()) \
        .LogState(oss.str().c_str());                                          \
  }
#define LOG_GENERAL(level, msg)                                                \
  {                                                                            \
    if (Logger::GetLogger(NULL, true,                                          \
                          boost::filesystem::absolute("./").string().c_str())  \
            .IsG3Log()) {                                                      \
      auto cur = std::chrono::system_clock::now();                             \
      auto cur_time_t = std::chrono::system_clock::to_time_t(cur);             \
      auto file_and_line =                                                     \
          std::string(std::string(__FILE__) + ":" + std::to_string(__LINE__)); \
      LOG(level) << "[" << PAD(Logger::GetPid(), Logger::TID_LEN, ' ') << "][" \
                 << std::put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")         \
                 << PAD(get_ms(cur), 3, '0') << "]["                           \
                 << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN)    \
                 << "][" << LIMIT(__FUNCTION__, Logger::MAX_FUNCNAME_LEN)      \
                 << "] " << msg;                                               \
    } else {                                                                   \
      std::ostringstream oss;                                                  \
      oss << msg;                                                              \
      Logger::GetLogger(NULL, true,                                            \
                        boost::filesystem::absolute("./").string().c_str())    \
          .LogGeneral(level, oss.str().c_str(), __LINE__, __FILE__,            \
                      __FUNCTION__);                                           \
    }                                                                          \
  }
#define LOG_EPOCH(level, epoch, msg)                                           \
  {                                                                            \
    if (Logger::GetLogger(NULL, true,                                          \
                          boost::filesystem::absolute("./").string().c_str())  \
            .IsG3Log()) {                                                      \
      auto cur = std::chrono::system_clock::now();                             \
      auto cur_time_t = std::chrono::system_clock::to_time_t(cur);             \
      auto file_and_line =                                                     \
          std::string(std::string(__FILE__) + ":" + std::to_string(__LINE__)); \
      LOG(level) << "[" << PAD(Logger::GetPid(), Logger::TID_LEN, ' ') << "][" \
                 << std::put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")         \
                 << PAD(get_ms(cur), 3, '0') << "]["                           \
                 << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN)    \
                 << "][" << LIMIT(__FUNCTION__, Logger::MAX_FUNCNAME_LEN)      \
                 << "] [Epoch " << std::to_string(epoch).c_str() << "] "       \
                 << msg;                                                       \
    } else {                                                                   \
      std::ostringstream oss;                                                  \
      oss << msg;                                                              \
      Logger::GetLogger(NULL, true,                                            \
                        boost::filesystem::absolute("./").string().c_str())    \
          .LogEpoch(level, std::to_string(epoch).c_str(), oss.str().c_str(),   \
                    __LINE__, __FILE__, __FUNCTION__);                         \
    }                                                                          \
  }
#define LOG_PAYLOAD(level, msg, payload, max_bytes_to_display)                 \
  {                                                                            \
    if (Logger::GetLogger(NULL, true,                                          \
                          boost::filesystem::absolute("./").string().c_str())  \
            .IsG3Log()) {                                                      \
      std::unique_ptr<char[]> payload_string;                                  \
      Logger::GetPayloadS(payload, max_bytes_to_display, payload_string);      \
      auto cur = std::chrono::system_clock::now();                             \
      auto cur_time_t = std::chrono::system_clock::to_time_t(cur);             \
      auto file_and_line =                                                     \
          std::string(std::string(__FILE__) + ":" + std::to_string(__LINE__)); \
      if ((payload).size() > max_bytes_to_display) {                           \
        LOG(level) << "[" << PAD(Logger::GetPid(), Logger::TID_LEN, ' ')       \
                   << "]["                                                     \
                   << std::put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")       \
                   << PAD(get_ms(cur), 3, '0') << "]["                         \
                   << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN)  \
                   << "][" << LIMIT(__FUNCTION__, Logger::MAX_FUNCNAME_LEN)    \
                   << "] " << msg << " (Len=" << (payload).size()              \
                   << "): " << payload_string.get() << "...";                  \
      } else {                                                                 \
        LOG(level) << "[" << PAD(Logger::GetPid(), Logger::TID_LEN, ' ')       \
                   << "]["                                                     \
                   << std::put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")       \
                   << PAD(get_ms(cur), 3, '0') << "]["                         \
                   << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN)  \
                   << "][" << LIMIT(__FUNCTION__, Logger::MAX_FUNCNAME_LEN)    \
                   << "] " << msg << " (Len=" << (payload).size()              \
                   << "): " << payload_string.get();                           \
      }                                                                        \
    } else {                                                                   \
      std::ostringstream oss;                                                  \
      oss << msg;                                                              \
      Logger::GetLogger(NULL, true,                                            \
                        boost::filesystem::absolute("./").string().c_str())    \
          .LogPayload(level, oss.str().c_str(), payload, max_bytes_to_display, \
                      __LINE__, __FILE__, __FUNCTION__);                       \
    }                                                                          \
  }
#define LOG_DISPLAY_LEVEL_ABOVE(level)                                    \
  {                                                                       \
    Logger::GetLogger(NULL, true,                                         \
                      boost::filesystem::absolute("./").string().c_str()) \
        .DisplayLevelAbove(level);                                        \
  }
#define LOG_ENABLE_LEVEL(level)                                           \
  {                                                                       \
    Logger::GetLogger(NULL, true,                                         \
                      boost::filesystem::absolute("./").string().c_str()) \
        .EnableLevel(level);                                              \
  }
#define LOG_DISABLE_LEVEL(level)                                          \
  {                                                                       \
    Logger::GetLogger(NULL, true,                                         \
                      boost::filesystem::absolute("./").string().c_str()) \
        .DisableLevel(level);                                             \
  }
#define LOG_EPOCHINFO(blockNum, msg)                                       \
  {                                                                        \
    std::ostringstream oss;                                                \
    oss << msg;                                                            \
    Logger::GetEpochInfoLogger(                                            \
        NULL, true, boost::filesystem::absolute("./").string().c_str())    \
        .LogEpochInfo(oss.str().c_str(), __LINE__, __FILE__, __FUNCTION__, \
                      std::to_string(blockNum).c_str());                   \
  }

#define LOG_CHECK_FAIL(checktype, received, expected) \
  LOG_GENERAL(WARNING, checktype << " check failed"); \
  LOG_GENERAL(WARNING, " Received = " << received);   \
  LOG_GENERAL(WARNING, " Expected = " << expected);

#endif  // ZILLIQA_SRC_LIBUTILS_LOGGER_H_
