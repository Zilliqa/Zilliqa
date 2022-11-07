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
#include <iomanip>
#include <sstream>
#include "common/BaseType.h"
#include "g3log/g3log.hpp"
#include "g3log/logworker.hpp"

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

  /// Returns the singleton instance for the logger.
  static Logger& GetLogger();

  //@{
  /// @name Sink addition.
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
  static void GetPayloadS(const bytes& payload, size_t max_bytes_to_display,
                          std::unique_ptr<char[]>& res);

  //@{
  /// @name Filter predicates (used internally by the below macros)
  static bool IsGeneralSink(g3::internal::SinkWrapper&, g3::LogMessage&);
  static bool IsStateSink(g3::internal::SinkWrapper&, g3::LogMessage&);
  static bool IsEpochInfoSink(g3::internal::SinkWrapper&, g3::LogMessage&);
  //@}

  //@{
  /// @name I/O stream manipulators.
  static std::ostream& CurrentTime(std::ostream&);
  static std::ostream& CurrentThreadId(std::ostream&);

  struct CodeLocation {
    CodeLocation(const char* file, int line, const char* func)
        : m_file{file}, m_line{line}, m_func{func} {}

    std::ostream& operator()(std::ostream&) const;

   protected:
    std::string m_file;
    int m_line;
    std::string m_func;
  };
  //@}

  struct ScopeMarker final : private CodeLocation {
    ScopeMarker(const char* file, int line, const char* func);
    ~ScopeMarker();

   private:
    ScopeMarker(const ScopeMarker&) = delete;
    ScopeMarker& operator=(const ScopeMarker&) = delete;
  };
};

namespace std {

inline ostream& operator<<(std::ostream& stream,
                           const Logger::CodeLocation& codeLocation) {
  return codeLocation(stream);
}
}  // namespace std

#define INIT_FILE_LOGGER(filePrefix, filePath) \
  Logger::GetLogger().AddGeneralSink(filePrefix, filePath);

#define INIT_STDOUT_LOGGER() Logger::GetLogger().AddStdoutSink();

#define INIT_STATE_LOGGER(filePrefix, filePath) \
  Logger::GetLogger().AddStateSink(filePrefix, filePath);

#define INIT_EPOCHINFO_LOGGER(filePrefix, filePath) \
  Logger::GetLogger().AddEpochInfoSink(filePrefix, filePath);

#define LOG_STATE(msg)                              \
  {                                                 \
    FILTERED_LOG(INFO, &Logger::IsStateSink)        \
        << Logger::CurrentTime << msg << std::endl; \
  }

#define INTERNAL_FILTERED_LOG_COMMON_BASE(level, pred, file, line, func)      \
  FILTERED_LOG(level, pred) << Logger::CurrentThreadId << Logger::CurrentTime \
                            << Logger::CodeLocation(file, line, func) << ' '

#define INTERNAL_FILTERED_LOG_COMMON(level, pred)                    \
  INTERNAL_FILTERED_LOG_COMMON_BASE(level, pred, __FILE__, __LINE__, \
                                    __FUNCTION__)

#define LOG_GENERAL(level, msg)                                 \
  {                                                             \
    INTERNAL_FILTERED_LOG_COMMON(level, &Logger::IsGeneralSink) \
        << msg << std::endl;                                    \
  }

#define LOG_MARKER() \
  Logger::ScopeMarker marker{__FILE__, __LINE__, __FUNCTION__};

#define LOG_EPOCH(level, epoch, msg)                                 \
  {                                                                  \
    INTERNAL_FILTERED_LOG_COMMON(level, &Logger::IsGeneralSink)      \
        << "[Epoch " << std::to_string(epoch).c_str() << "] " << msg \
        << std::endl;                                                \
  }

#define LOG_PAYLOAD(level, msg, payload, max_bytes_to_display)          \
  {                                                                     \
    std::unique_ptr<char[]> payload_string;                             \
    Logger::GetPayloadS(payload, max_bytes_to_display, payload_string); \
    INTERNAL_FILTERED_LOG_COMMON(level, &Logger::IsGeneralSink)         \
        << msg << " (Len=" << (payload).size()                          \
        << "): " << payload_string.get()                                \
        << (((payload).size() > max_bytes_to_display) ? "..." : "")     \
        << std::endl;                                                   \
  }

#define LOG_DISPLAY_LEVEL_ABOVE(level) \
  { Logger::GetLogger().DisplayLevelAbove(level); }

#define LOG_EPOCHINFO(blockNum, msg)                                          \
  {                                                                           \
    INTERNAL_FILTERED_LOG_COMMON(INFO, &Logger::IsEpochInfoSink)              \
        << "[Epoch " << std::to_string(blockNum) << "] " << msg << std::endl; \
  }

#define LOG_CHECK_FAIL(checktype, received, expected) \
  LOG_GENERAL(WARNING, checktype << " check failed"); \
  LOG_GENERAL(WARNING, " Received = " << received);   \
  LOG_GENERAL(WARNING, " Expected = " << expected);

#endif  // ZILLIQA_SRC_LIBUTILS_LOGGER_H_
