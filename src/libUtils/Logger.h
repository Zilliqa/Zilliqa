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

#include <boost/filesystem/path.hpp>
#include <boost/scope_exit.hpp>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include "common/BaseType.h"
#include "g3log/g3log.hpp"
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
  std::unique_ptr<g3::LogWorker> m_logWorker;

  Logger();

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
  static const int MAX_FILE_SIZE;

  /// Returns the singleton instance for the main Logger.
  static Logger& GetLogger();

  void AddGeneralSink(const std::string& filePrefix,
                      const boost::filesystem::path& filePath,
                      int maxFileSize = MAX_FILE_SIZE);

  void AddStateSink(const std::string& filePrefix,
                    const boost::filesystem::path& filePath,
                    int maxFileSize = MAX_FILE_SIZE);

  void AddEpochInfoSink(const std::string& filePrefix,
                        const boost::filesystem::path& filePath,
                        int maxFileSize = MAX_FILE_SIZE);

  void AddStdoutSink();

  /// Setup the display debug level
  ///     INFO: display all message
  ///     WARNING: display warning and fatal message
  ///     FATAL: display fatal message only
  void DisplayLevelAbove(const LEVELS& level = INFO);

  /// Enable the log level
  void EnableLevel(const LEVELS& level);

  /// Disable the log level
  void DisableLevel(const LEVELS& level);

  /// Get current process id
  static pid_t GetPid();

  /// Calculate payload string according to payload vector & length
  static void GetPayloadS(const bytes& payload, size_t max_bytes_to_display,
                          std::unique_ptr<char[]>& res);

  static bool IsGeneralSink(g3::internal::SinkWrapper&, g3::LogMessage&);
  static bool IsStateSink(g3::internal::SinkWrapper&, g3::LogMessage&);
  static bool IsEpochInfoSink(g3::internal::SinkWrapper&, g3::LogMessage&);
};

#define INIT_FILE_LOGGER(filePrefix, filePath) \
  Logger::GetLogger().AddGeneralSink(filePrefix, filePath);

#define INIT_STDOUT_LOGGER() Logger::GetLogger().AddStdoutSink();

#define INIT_STATE_LOGGER(filePrefix, filePath) \
  Logger::GetLogger().AddStateSink(filePrefix, filePath);

#define INIT_EPOCHINFO_LOGGER(filePrefix, filePath) \
  Logger::GetLogger().AddEpochInfoSink(filePrefix, filePath);

#define LOG_STATE(msg)                                                \
  {                                                                   \
    auto cur = std::chrono::system_clock::now();                      \
    auto cur_time_t = std::chrono::system_clock::to_time_t(cur);      \
    FILTERED_LOG(INFO, &Logger::IsStateSink)                          \
        << "[ " << std::put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.") \
        << PAD(get_ms(cur), 3, '0') << " ]" << msg;                   \
  }

#define LOG_GENERAL(level, msg)                                            \
  {                                                                        \
    auto cur = std::chrono::system_clock::now();                           \
    auto cur_time_t = std::chrono::system_clock::to_time_t(cur);           \
    auto file_and_line =                                                   \
        std::string(__FILE__) + ":" + std::to_string(__LINE__);            \
    FILTERED_LOG(level, &Logger::IsGeneralSink)                            \
        << "[" << PAD(Logger::GetPid(), Logger::TID_LEN, ' ') << "]["      \
        << std::put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")              \
        << PAD(get_ms(cur), 3, '0') << "]["                                \
        << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "][" \
        << LIMIT(__FUNCTION__, Logger::MAX_FUNCNAME_LEN) << "] " << msg    \
        << std::endl;                                                      \
  }

#define LOG_MARKER()                                                    \
  BOOST_SCOPE_EXIT(void){LOG_GENERAL(INFO, "END")} BOOST_SCOPE_EXIT_END \
  LOG_GENERAL(INFO, "BEG")

#define LOG_EPOCH(level, epoch, msg)                                       \
  {                                                                        \
    auto cur = std::chrono::system_clock::now();                           \
    auto cur_time_t = std::chrono::system_clock::to_time_t(cur);           \
    auto file_and_line =                                                   \
        std::string(__FILE__) + ":" + std::to_string(__LINE__);            \
    FILTERED_LOG(level, &Logger::IsGeneralSink)                            \
        << "[" << PAD(Logger::GetPid(), Logger::TID_LEN, ' ') << "]["      \
        << std::put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")              \
        << PAD(get_ms(cur), 3, '0') << "]["                                \
        << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "][" \
        << LIMIT(__FUNCTION__, Logger::MAX_FUNCNAME_LEN) << "] [Epoch "    \
        << std::to_string(epoch).c_str() << "] " << msg;                   \
  }

#define LOG_PAYLOAD(level, msg, payload, max_bytes_to_display)               \
  {                                                                          \
    std::unique_ptr<char[]> payload_string;                                  \
    Logger::GetPayloadS(payload, max_bytes_to_display, payload_string);      \
    auto cur = std::chrono::system_clock::now();                             \
    auto cur_time_t = std::chrono::system_clock::to_time_t(cur);             \
    auto file_and_line =                                                     \
        std::string(__FILE__) + ":" + std::to_string(__LINE__);              \
    if ((payload).size() > max_bytes_to_display) {                           \
      FILTERED_LOG(level, &Logger::IsGeneralSink)                            \
          << "[" << PAD(Logger::GetPid(), Logger::TID_LEN, ' ') << "]["      \
          << std::put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")              \
          << PAD(get_ms(cur), 3, '0') << "]["                                \
          << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "][" \
          << LIMIT(__FUNCTION__, Logger::MAX_FUNCNAME_LEN) << "] " << msg    \
          << " (Len=" << (payload).size() << "): " << payload_string.get()   \
          << "...";                                                          \
    } else {                                                                 \
      FILTERED_LOG(level, &Logger::IsGeneralSink)                            \
          << "[" << PAD(Logger::GetPid(), Logger::TID_LEN, ' ') << "]["      \
          << std::put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")              \
          << PAD(get_ms(cur), 3, '0') << "]["                                \
          << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "][" \
          << LIMIT(__FUNCTION__, Logger::MAX_FUNCNAME_LEN) << "] " << msg    \
          << " (Len=" << (payload).size() << "): " << payload_string.get();  \
    }                                                                        \
  }

#define LOG_DISPLAY_LEVEL_ABOVE(level) \
  { Logger::GetLogger().DisplayLevelAbove(level); }

#define LOG_EPOCHINFO(blockNum, msg)                                       \
  {                                                                        \
    auto cur = chrono::system_clock::now();                                \
    auto cur_time_t = chrono::system_clock::to_time_t(cur);                \
    auto file_and_line =                                                   \
        std::string(__FILE__) + ":" + std::to_string(__LINE__);            \
    FILTERED_LOG(INFO, &Logger::IsEpochInfoSink)                           \
        << "[" << PAD(Logger::GetPid(), Logger::TID_LEN, ' ') << "]["      \
        << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")                   \
        << PAD(get_ms(cur), 3, '0') << "]["                                \
        << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "][" \
        << LIMIT(__FUNCTION__, Logger::MAX_FUNCNAME_LEN) << "] [Epoch "    \
        << std::to_string(blockNum) << "] " << msg;                        \
  }

#define LOG_CHECK_FAIL(checktype, received, expected) \
  LOG_GENERAL(WARNING, checktype << " check failed"); \
  LOG_GENERAL(WARNING, " Received = " << received);   \
  LOG_GENERAL(WARNING, " Expected = " << expected);

#endif  // ZILLIQA_SRC_LIBUTILS_LOGGER_H_
