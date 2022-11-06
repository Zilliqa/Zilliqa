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

#include "Logger.h"

#include <g3sinks/LogRotate.h>

#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <boost/filesystem/operations.hpp>
#include <cstring>
#include <iostream>

using namespace std;
using namespace g3;

inline string MyCustomFormatting(const LogMessage& msg) {
  return string("[") + msg.level().substr(0, 4) + "]";
}

namespace {
/// helper function to get tid with better cross-platform support
inline pid_t getCurrentPid() {
#if defined(__linux__)
  return syscall(SYS_gettid);
#elif defined(__APPLE__) && defined(__MACH__)
  uint64_t tid64;
  pthread_threadid_np(NULL, &tid64);
  return static_cast<pid_t>(tid64);
#else
#error  // not implemented in this platform
  return 0;
#endif
}

template <typename LogRotateSinkT>
void AddFileSink(g3::LogWorker& logWorker, const std::string& filePrefix,
                 const boost::filesystem::path& filePath,
                 std::size_t maxFileSize /*= MAX_FILE_SIZE*/) {
  assert(!filePath.has_filename());

  auto logFileRoot = boost::filesystem::absolute(filePath);
  bool useDefaultLocation = false;
  try {
    if (!boost::filesystem::create_directory(logFileRoot)) {
      if ((boost::filesystem::status(logFileRoot).permissions() &
           boost::filesystem::perms::owner_write) ==
          boost::filesystem::perms::no_perms) {
        useDefaultLocation = true;
        std::cout << logFileRoot
                  << " already existed but no writing permission!" << endl;
      }
    }
  } catch (const boost::filesystem::filesystem_error& e) {
    std::cout << "Cannot create log folder in " << logFileRoot
              << ", error code: " << e.code() << endl;
    useDefaultLocation = true;
  }

  if (useDefaultLocation) {
    logFileRoot = boost::filesystem::absolute("./");
    std::cout << "Use default log folder " << logFileRoot << " instead."
              << endl;
  }

  auto sinkHandle = logWorker.addSink(
      std::make_unique<LogRotateSinkT>(
          filePrefix.empty() ? "common" : filePrefix, logFileRoot.c_str()),
      &LogRotateSinkT::receiveLogMessage);
  sinkHandle->call(&LogRotateSinkT::setMaxLogSize, maxFileSize).wait();
}

class CustomLogRotate {
 public:
  virtual ~CustomLogRotate() noexcept = default;

  template <typename... ArgsT>
  CustomLogRotate(ArgsT&&... args)
      : m_logRotate(std::forward<ArgsT>(args)...) {}

  void setMaxLogSize(int max_file_size_in_bytes) {
    m_logRotate.setMaxLogSize(max_file_size_in_bytes);
  }

  void receiveLogMessage(g3::LogMessageMover logEntry) {
    m_logRotate.save(logEntry.get().message());
  }

 private:
  LogRotate m_logRotate;
};

class GeneralLogSink : public CustomLogRotate {
 public:
  using CustomLogRotate::CustomLogRotate;
};

class StateLogSink : public CustomLogRotate {
 public:
  using CustomLogRotate::CustomLogRotate;
};

class EpochLogSink : public CustomLogRotate {
 public:
  using CustomLogRotate::CustomLogRotate;
};

class StdoutSink {
 public:
  void forwardLogToStdout(g3::LogMessageMover logEntry) {
    std::cout << logEntry.get().message();
  }
};

}  // namespace

const streampos Logger::MAX_FILE_SIZE =
    1024 * 1024 * 100;  // 100MB per log file

Logger::Logger() : m_logWorker{LogWorker::createLogWorker()} {
  initializeLogging(m_logWorker.get());
}

void Logger::AddGeneralSink(const std::string& filePrefix,
                            const boost::filesystem::path& filePath,
                            std::size_t maxFileSize /*= MAX_FILE_SIZE*/) {
  AddFileSink<GeneralLogSink>(*m_logWorker, filePrefix, filePath, maxFileSize);
}

void Logger::AddStateSink(const std::string& filePrefix,
                          const boost::filesystem::path& filePath,
                          std::size_t maxFileSize /*= MAX_FILE_SIZE*/) {
  AddFileSink<StateLogSink>(*m_logWorker, filePrefix, filePath, maxFileSize);
}

void Logger::AddEpochSink(const std::string& filePrefix,
                          const boost::filesystem::path& filePath,
                          std::size_t maxFileSize /*= MAX_FILE_SIZE*/) {
  AddFileSink<EpochLogSink>(*m_logWorker, filePrefix, filePath, maxFileSize);
}

void Logger::AddStdoutSink() {
  m_logWorker->addSink(std::make_unique<StdoutSink>(),
                       &StdoutSink::forwardLogToStdout);
}

Logger& Logger::GetLogger() {
  static Logger logger;
  return logger;
}

void Logger::LogState(const char* msg) { LOG(INFO) << msg << endl << flush; }

#if 0
void Logger::LogGeneral(const LEVELS& level, const char* msg,
                        const unsigned int linenum, const char* filename,
                        const char* function) {
  auto cur = chrono::system_clock::now();
  auto cur_time_t = chrono::system_clock::to_time_t(cur);
  auto file_and_line =
      std::string(std::string(filename) + ":" + std::to_string(linenum));
  LOG(level) << "[" << PAD(GetPid(), TID_LEN, ' ') << "]["
             << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
             << PAD(get_ms(cur), 3, '0') << "]["
             << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "]["
             << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg;
}
#endif

void Logger::LogEpoch(const LEVELS& level, const char* msg, const char* epoch,
                      const unsigned int linenum, const char* filename,
                      const char* function) {
  auto cur = chrono::system_clock::now();
  auto cur_time_t = chrono::system_clock::to_time_t(cur);
  auto file_and_line =
      std::string(std::string(filename) + ":" + std::to_string(linenum));
  LOG(level) << "[" << PAD(GetPid(), TID_LEN, ' ') << "]["
             << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
             << PAD(get_ms(cur), 3, '0') << "]["
             << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "]["
             << LIMIT(function, MAX_FUNCNAME_LEN) << "] [Epoch " << epoch
             << "] " << msg;
}

void Logger::LogPayload(const LEVELS& level, const char* msg,
                        const bytes& payload, size_t max_bytes_to_display,
                        const unsigned int linenum, const char* filename,
                        const char* function) {
  std::unique_ptr<char[]> payload_string;
  GetPayloadS(payload, max_bytes_to_display, payload_string);

  auto cur = chrono::system_clock::now();
  auto cur_time_t = chrono::system_clock::to_time_t(cur);
  auto file_and_line = std::string(filename) + ":" + std::to_string(linenum);

  LOG(level) << "[" << PAD(GetPid(), TID_LEN, ' ') << "]["
             << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
             << PAD(get_ms(cur), 3, '0') << "]["
             << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "]["
             << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg
             << " (Len=" << payload.size() << "): " << payload_string.get()
             << (payload.size() > max_bytes_to_display ? "..." : "");
}

void Logger::LogEpochInfo(const char* msg, const unsigned int linenum,
                          const char* filename, const char* function,
                          const char* epoch) {
  pid_t tid = getCurrentPid();
  auto cur = chrono::system_clock::now();
  auto cur_time_t = chrono::system_clock::to_time_t(cur);
  auto file_and_line =
      std::string(std::string(filename) + ":" + std::to_string(linenum));
  LOG(INFO) << "[" << PAD(tid, TID_LEN, ' ') << "]["
            << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
            << PAD(get_ms(cur), 3, '0') << "]["
            << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "]["
            << LIMIT(function, MAX_FUNCNAME_LEN) << "] [Epoch " << epoch << "] "
            << msg;
}

void Logger::DisplayLevelAbove(const LEVELS& level) {
  if (level != INFO && level != WARNING && level != FATAL) return;

  g3::log_levels::setHighest(level);
}

void Logger::EnableLevel(const LEVELS& level) { g3::log_levels::enable(level); }

void Logger::DisableLevel(const LEVELS& level) {
  g3::log_levels::disable(level);
}

pid_t Logger::GetPid() { return getCurrentPid(); }

void Logger::GetPayloadS(const bytes& payload, size_t max_bytes_to_display,
                         std::unique_ptr<char[]>& res) {
  static const char* hex_table = "0123456789ABCDEF";
  size_t payload_string_len = (payload.size() * 2) + 1;

  if (payload.size() > max_bytes_to_display) {
    payload_string_len = (max_bytes_to_display * 2) + 1;
  }

  res = make_unique<char[]>(payload_string_len);

  for (unsigned int payload_idx = 0, payload_string_idx = 0;
       (payload_idx < payload.size()) &&
       ((payload_string_idx + 2) < payload_string_len);
       payload_idx++) {
    res.get()[payload_string_idx++] =
        hex_table[(payload.at(payload_idx) >> 4) & 0xF];
    res.get()[payload_string_idx++] = hex_table[payload.at(payload_idx) & 0xF];
  }

  res.get()[payload_string_len - 1] = '\0';
}

ScopeMarker::ScopeMarker(const unsigned int linenum, const char* filename,
                         const char* function)
    : m_linenum(linenum), m_filename(filename), m_function(function) {
#if 0
  Logger::GetLogger().LogGeneral(INFO, "BEG", linenum, filename, function);
#endif
}

ScopeMarker::~ScopeMarker() {
#if 0
  Logger::GetLogger().LogGeneral(INFO, "END", m_linenum, m_filename.c_str(),
                                 m_function.c_str());
#endif
}
