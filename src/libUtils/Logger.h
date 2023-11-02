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

#include "common/Constants.h"
#include "g3log/logworker.hpp"

#include <filesystem>
#include <typeinfo>

#define PAD(n, len, ch) std::setw(len) << std::setfill(ch) << std::right << n

/// Utility logging class for outputting messages to stdout or file.
class Logger {
  std::unique_ptr<g3::LogWorker> m_logWorker;
  static std::vector<std::reference_wrapper<const std::type_info>>
      m_externalSinkTypeIds;

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

  /// Returns the singleton instance for the logger.
  static Logger& GetLogger();

  //@{
  /// @name Sink addition.
  void AddGeneralSink(const std::string& filePrefix,
                      const std::filesystem::path& filePath,
                      int maxLogFileSizeKB = MAX_LOG_FILE_SIZE_KB,
                      int maxArchivedLogCount = MAX_ARCHIVED_LOG_COUNT);

  void AddStateSink(const std::string& filePrefix,
                    const std::filesystem::path& filePath,
                    int maxLogFileSizeKB = MAX_LOG_FILE_SIZE_KB,
                    int maxArchivedLogCount = MAX_ARCHIVED_LOG_COUNT);

  void AddEpochInfoSink(const std::string& filePrefix,
                        const std::filesystem::path& filePath,
                        int maxLogFileSizeKB = MAX_LOG_FILE_SIZE_KB,
                        int maxArchivedLogCount = MAX_ARCHIVED_LOG_COUNT);

  void AddJsonSink(const std::string& filePrefix,
                   const std::filesystem::path& filePath,
                   int maxLogFileSizeKB = MAX_LOG_FILE_SIZE_KB,
                   int maxArchivedLogCount = MAX_ARCHIVED_LOG_COUNT);

  void AddStdoutSink();

  template <typename SinkT, typename MemFuncT>
  void AddSink(std::unique_ptr<SinkT> sink, MemFuncT memFunc) {
    auto handle = m_logWorker->addSink(std::move(sink), memFunc);
    if (handle) {
      m_externalSinkTypeIds.emplace_back(typeid(g3::internal::Sink<SinkT>));
    }
  }
  //@}

  /// Setup the display debug level
  ///     INFO: display all message
  ///     WARNING: display warning and fatal message
  ///     FATAL: display fatal message only
  void DisplayLevelAbove(const LEVELS& level = INFO);

  /// Enable the log level
  void EnableLevel(const LEVELS& level);

  /// Disable the log level
  void DisableLevel(const LEVELS& level);

  /// Calculate payload string according to payload vector & length
  static void GetPayloadS(const zbytes& payload, size_t max_bytes_to_display,
                          std::unique_ptr<char[]>& res);

  //@{
  /// @name Filter predicates (used internally by the below macros)

  // Note that all the predicates return true for the stdout sink as well
  // so all logs are also printed to std::cout.
  static bool IsGeneralSink(g3::internal::SinkWrapper&, g3::LogMessage&);
  static bool IsStateSink(g3::internal::SinkWrapper&, g3::LogMessage&);
  static bool IsEpochInfoSink(g3::internal::SinkWrapper&, g3::LogMessage&);
  //@}

  // Auxiliary class to mark the beginning & end of a scope.
  struct ScopeMarker final {
    ScopeMarker(const char* file, int line, const char* func,
                bool should_print = true);
    ~ScopeMarker();

   private:
    std::string m_file;
    int m_line;
    std::string m_func;
    bool should_print;

    ScopeMarker(const ScopeMarker&) = delete;
    ScopeMarker& operator=(const ScopeMarker&) = delete;
  };
};

std::shared_ptr<g3::ExtraData> CreateTracingExtraData();

#define TRACED_FILTERED_INTERNAL_LOG_MESSAGE(level, pred)                \
  LogCapture(__FILE__, __LINE__,                                         \
             static_cast<const char*>(__PRETTY_FUNCTION__), level, pred, \
             CreateTracingExtraData())

#define TRACED_FILTERED_LOG(level, pred) \
  if (!g3::logLevel(level)) {            \
  } else                                 \
    TRACED_FILTERED_INTERNAL_LOG_MESSAGE(level, pred).stream()

#define INIT_FILE_LOGGER(filePrefix, filePath) \
  Logger::GetLogger().AddGeneralSink(filePrefix, filePath);

#define INIT_STDOUT_LOGGER() Logger::GetLogger().AddStdoutSink();

#define INIT_STATE_LOGGER(filePrefix, filePath) \
  Logger::GetLogger().AddStateSink(filePrefix, filePath);

#define INIT_EPOCHINFO_LOGGER(filePrefix, filePath) \
  Logger::GetLogger().AddEpochInfoSink(filePrefix, filePath);

#define INIT_JSON_LOGGER(filePrefix, filePath) \
  Logger::GetLogger().AddJsonSink(filePrefix, filePath);

#define LOG_STATE(msg) \
  { TRACED_FILTERED_LOG(INFO, &Logger::IsStateSink) << ' ' << msg; }

#define LOG_GENERAL(level, msg) \
  { TRACED_FILTERED_LOG(level, &Logger::IsGeneralSink) << ' ' << msg; }

#define NOMARK 1
#if !defined(NOMARK)
#define LOG_MARKER() \
  Logger::ScopeMarker marker{__FILE__, __LINE__, __FUNCTION__};
#else
#define LOG_MARKER()
#endif

#define LOG_MARKER_CONTITIONAL(conditional) \
  Logger::ScopeMarker marker{__FILE__, __LINE__, __FUNCTION__, conditional};

#define LOG_EPOCH(level, epoch, msg)                                  \
  {                                                                   \
    TRACED_FILTERED_LOG(level, &Logger::IsGeneralSink)                \
        << "[Epoch " << std::to_string(epoch).c_str() << "] " << msg; \
  }

#define LOG_PAYLOAD(level, msg, payload, max_bytes_to_display)          \
  {                                                                     \
    std::unique_ptr<char[]> payload_string;                             \
    Logger::GetPayloadS(payload, max_bytes_to_display, payload_string); \
    TRACED_FILTERED_LOG(level, &Logger::IsGeneralSink)                  \
        << ' ' << msg << " (Len=" << (payload).size()                   \
        << "): " << payload_string.get()                                \
        << (((payload).size() > max_bytes_to_display) ? "..." : "");    \
  }

#define LOG_DISPLAY_LEVEL_ABOVE(level) \
  { Logger::GetLogger().DisplayLevelAbove(level); }

#define LOG_EPOCHINFO(blockNum, msg)                             \
  {                                                              \
    TRACED_FILTERED_LOG(INFO, &Logger::IsEpochInfoSink)          \
        << "[Epoch " << std::to_string(blockNum) << "] " << msg; \
  }

#define LOG_CHECK_FAIL(checktype, received, expected) \
  LOG_GENERAL(WARNING, checktype << " check failed"); \
  LOG_GENERAL(WARNING, " Received = " << received);   \
  LOG_GENERAL(WARNING, " Expected = " << expected);

#define LOG_EXTRA_ENABLED 1

#if LOG_EXTRA_ENABLED
#define LOG_EXTRA(msg) LOG_GENERAL(INFO, "### " << msg)
#else
#define LOG_EXTRA(...)
#endif

#endif  // ZILLIQA_SRC_LIBUTILS_LOGGER_H_
