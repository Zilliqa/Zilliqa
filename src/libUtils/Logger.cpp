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

#if 0
inline string MyCustomFormatting(const LogMessage& msg) {
  return string("[") + msg.level().substr(0, 4) + "]";
}
#endif

namespace {
/// helper function to get tid with better cross-platform support
inline pid_t getCurrentTid() {
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
                 int maxFileSize /*= MAX_FILE_SIZE*/) {
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

class EpochInfoLogSink : public CustomLogRotate {
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

const int Logger::MAX_FILE_SIZE = 1024 * 1024 * 100;  // 100MB per log file

Logger::Logger() : m_logWorker{LogWorker::createLogWorker()} {
  initializeLogging(m_logWorker.get());
}

void Logger::AddGeneralSink(const std::string& filePrefix,
                            const boost::filesystem::path& filePath,
                            int maxFileSize /*= MAX_FILE_SIZE*/) {
  AddFileSink<GeneralLogSink>(*m_logWorker, filePrefix, filePath, maxFileSize);
}

void Logger::AddStateSink(const std::string& filePrefix,
                          const boost::filesystem::path& filePath,
                          int maxFileSize /*= MAX_FILE_SIZE*/) {
  AddFileSink<StateLogSink>(*m_logWorker, filePrefix, filePath, maxFileSize);
}

void Logger::AddEpochInfoSink(const std::string& filePrefix,
                              const boost::filesystem::path& filePath,
                              int maxFileSize /*= MAX_FILE_SIZE*/) {
  AddFileSink<EpochInfoLogSink>(*m_logWorker, filePrefix, filePath,
                                maxFileSize);
}

void Logger::AddStdoutSink() {
  m_logWorker->addSink(std::make_unique<StdoutSink>(),
                       &StdoutSink::forwardLogToStdout);
}

bool Logger::IsGeneralSink(g3::internal::SinkWrapper& sink, g3::LogMessage&) {
  return typeid(sink) == typeid(g3::internal::Sink<GeneralLogSink>) ||
         typeid(sink) == typeid(g3::internal::Sink<StdoutSink>);
}

bool Logger::IsStateSink(g3::internal::SinkWrapper& sink, g3::LogMessage&) {
  return typeid(sink) == typeid(g3::internal::Sink<StateLogSink>) ||
         typeid(sink) == typeid(g3::internal::Sink<StdoutSink>);
}

bool Logger::IsEpochInfoSink(g3::internal::SinkWrapper& sink, g3::LogMessage&) {
  return typeid(sink) == typeid(g3::internal::Sink<EpochInfoLogSink>) ||
         typeid(sink) == typeid(g3::internal::Sink<StdoutSink>);
}

Logger& Logger::GetLogger() {
  static Logger logger;
  return logger;
}

void Logger::DisplayLevelAbove(const LEVELS& level) {
  if (level != INFO && level != WARNING && level != FATAL) return;

  g3::log_levels::setHighest(level);
}

void Logger::EnableLevel(const LEVELS& level) { g3::log_levels::enable(level); }

void Logger::DisableLevel(const LEVELS& level) {
  g3::log_levels::disable(level);
}

pid_t Logger::GetPid() { return getCurrentTid(); }

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

#define LIMIT(s, len)                              \
  std::setw(len) << std::setfill(' ') << std::left \
                 << std::string(s).substr(0, len)

#define LIMIT_RIGHT(s, len)                        \
  std::setw(len) << std::setfill(' ') << std::left \
                 << s.substr(std::max<int>((int)s.size() - len, 0))

std::ostream& Logger::CurrentTime(std::ostream& stream) {
  auto cur = std::chrono::system_clock::now();
  auto cur_time_t = std::chrono::system_clock::to_time_t(cur);
  stream << "[ " << std::put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
         << PAD(get_ms(cur), 3, '0') << " ]";
  return stream;
}

std::ostream& Logger::CurrentThreadId(std::ostream& stream) {
  stream << '[' << PAD(getCurrentTid(), Logger::TID_LEN, ' ') << ']';
  return stream;
}

std::ostream& Logger::CodeLocation::operator()(std::ostream& stream) const {
  auto fileAndLine = m_file + ':' + std::to_string(m_line);
  stream << '[' << LIMIT_RIGHT(fileAndLine, Logger::MAX_FILEANDLINE_LEN) << "]["
         << LIMIT(m_func, Logger::MAX_FUNCNAME_LEN) << ']';
  return stream;
}
