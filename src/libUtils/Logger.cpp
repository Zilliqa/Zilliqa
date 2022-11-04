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

class StdoutSink {
 public:
  void forwardLogToStdout(g3::LogMessageMover logEntry) {
    std::cout << logEntry.get().message() << std::endl;
  }
};

}  // namespace

const streampos Logger::MAX_FILE_SIZE =
    1024 * 1024 * 100;  // 100MB per log file

Logger::Logger() : m_logWorker{LogWorker::createLogWorker()} {
  initializeLogging(m_logWorker.get());
#if 0
  this->m_logToFile = log_to_file;
  this->m_maxFileSize = max_file_size;
  this->m_logPath = logpath;
#endif

#if 0
  try {
    if (!boost::filesystem::create_directory(this->m_logPath)) {
      if ((boost::filesystem::status(this->m_logPath).permissions() &
           boost::filesystem::perms::owner_write) ==
          boost::filesystem::perms::no_perms) {
        std::cout << this->m_logPath
                  << " already existed but no writing permission!" << endl;
        this->m_logPath = boost::filesystem::absolute("./").string();
        std::cout << "Use default log folder " << this->m_logPath << " instead."
                  << endl;
      }
    }
  } catch (const boost::filesystem::filesystem_error& e) {
    std::cout << "Cannot create log folder in " << this->m_logPath
              << ", error code: " << e.code() << endl;
    this->m_logPath = boost::filesystem::absolute("./").string();
    std::cout << "Use default log folder " << this->m_logPath << " instead."
              << endl;
  }

  if (log_to_file) {
    m_fileNamePrefix = prefix ? prefix : "common";
    m_seqNum = 0;
    newLog();
  }
#endif
}

void Logger::LogToFile(const boost::filesystem::path& filePath,
                       std::size_t maxFileSize /*= MAX_FILE_SIZE*/) {
  auto logFileRoot = boost::filesystem::absolute(filePath.root_path());
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

  auto logFileName = filePath.filename();
  if (logFileName.empty()) logFileName = "common.log";

  auto sinkHandle = m_logWorker->addSink(
      std::make_unique<LogRotate>(logFileName.c_str(), logFileRoot.c_str()),
      &LogRotate::save);
  sinkHandle->call(&LogRotate::setMaxLogSize, maxFileSize).wait();
}

void Logger::LogToConsole() {
  m_logWorker->addSink(std::make_unique<StdoutSink>(),
                       &StdoutSink::forwardLogToStdout);
}

#if 0
void Logger::newLog() {
  // m_seqNum++;
  m_bRefactor = (m_fileNamePrefix == "zilliqa");

  using boost::format;
  // Filename = m_fileNamePrefix + 5-digit sequence number + timestamp + ".log"
  m_fileName = m_fileNamePrefix + str(format("-%05d") % m_seqNum);

  if (m_bRefactor) {
    auto sinkHandle = m_logWorker->addSink(
        std::make_unique<FileSink>(m_fileName.c_str(), m_logPath, ""),
        &FileSink::fileWrite);
    sinkHandle->call(&g3::FileSink::overrideLogDetails, &MyCustomFormatting)
        .wait();
    sinkHandle->call(&g3::FileSink::overrideLogHeader, "").wait();

    auto sinkHandle = m_logWorker->addSink(
        std::make_unique<LogRotate>(m_fileName.c_str(), m_logPath),
        &LogRotate::save);
    sinkHandle->call(&LogRotate::setMaxLogSize, MAX_FILE_SIZE).wait();

    initializeLogging(m_logWorker.get());
  } else {
    m_fileName += ".log";
    m_logFile.open(m_logPath + m_fileName, ios_base::app);
  }
}
#endif

Logger& Logger::GetLogger() {
  static Logger logger;
  return logger;
}

Logger& Logger::GetStateLogger() {
  static Logger logger;
  return logger;
}

Logger& Logger::GetEpochInfoLogger() {
  static Logger logger;
  return logger;
}

void Logger::LogState(const char* msg) { LOG(INFO) << msg << endl << flush; }

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
  Logger::GetLogger().LogGeneral(INFO, "BEG", linenum, filename, function);
}

ScopeMarker::~ScopeMarker() {
  Logger::GetLogger().LogGeneral(INFO, "END", m_linenum, m_filename.c_str(),
                                 m_function.c_str());
}
