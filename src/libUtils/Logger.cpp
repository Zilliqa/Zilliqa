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

#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
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
};  // namespace

const streampos Logger::MAX_FILE_SIZE =
    1024 * 1024 * 100;  // 100MB per log file

Logger::Logger(const char* prefix, bool log_to_file, streampos max_file_size) {
  this->m_logToFile = log_to_file;
  this->m_maxFileSize = max_file_size;

  if (log_to_file) {
    m_fileNamePrefix = prefix ? prefix : "common";
    m_seqNum = 0;
    newLog();
  }
}

Logger::~Logger() { m_logFile.close(); }

void Logger::checkLog() {
  std::ifstream in(m_fileName.c_str(),
                   std::ifstream::ate | std::ifstream::binary);

  if (in.tellg() >= m_maxFileSize) {
    m_logFile.close();
    newLog();
  }
}

void Logger::newLog() {
  m_seqNum++;
  m_bRefactor = (m_fileNamePrefix == "zilliqa");

  // Filename = m_fileNamePrefix + 5-digit sequence number + "-log.txt"
  char buf[16] = {0};
  snprintf(buf, sizeof(buf), (m_bRefactor ? "-%05d-log" : "-%05d-log.txt"),
           m_seqNum);
  m_fileName = m_fileNamePrefix + buf;

  if (m_bRefactor) {
    logworker = LogWorker::createLogWorker();
    auto sinkHandle = logworker->addSink(
        std::make_unique<FileSink>(m_fileName.c_str(), "./", ""),
        &FileSink::fileWrite);
    sinkHandle->call(&g3::FileSink::overrideLogDetails, &MyCustomFormatting)
        .wait();
    sinkHandle->call(&g3::FileSink::overrideLogHeader, "").wait();
    initializeLogging(logworker.get());
  } else {
    m_logFile.open(m_fileName.c_str(), ios_base::app);
  }
}

Logger& Logger::GetLogger(const char* fname_prefix, bool log_to_file,
                          streampos max_file_size) {
  static Logger logger(fname_prefix, log_to_file, max_file_size);
  return logger;
}

Logger& Logger::GetStateLogger(const char* fname_prefix, bool log_to_file,
                               streampos max_file_size) {
  static Logger logger(fname_prefix, log_to_file, max_file_size);
  return logger;
}

Logger& Logger::GetEpochInfoLogger(const char* fname_prefix, bool log_to_file,
                                   streampos max_file_size) {
  static Logger logger(fname_prefix, log_to_file, max_file_size);
  return logger;
}

void Logger::LogState(const char* msg) {
  lock_guard<mutex> guard(m);

  if (m_logToFile) {
    checkLog();
    m_logFile << msg << endl << flush;
  } else {
    cout << msg << endl << flush;
  }
}

void Logger::LogGeneral(LEVELS level, const char* msg,
                        const unsigned int linenum, const char* filename,
                        const char* function) {
  if (IsG3Log()) {
    auto cur = chrono::system_clock::now();
    auto cur_time_t = chrono::system_clock::to_time_t(cur);
    auto file_and_line =
        std::string(std::string(filename) + ":" + std::to_string(linenum));
    LOG(level) << "[" << PAD(GetPid(), TID_LEN, ' ') << "]["
               << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
               << PAD(get_ms(cur), 3, '0') << "]["
               << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN)
               << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg;
    return;
  }

  lock_guard<mutex> guard(m);

  if (m_logToFile) {
    checkLog();
    auto cur = chrono::system_clock::now();
    auto cur_time_t = chrono::system_clock::to_time_t(cur);
    auto file_and_line =
        std::string(std::string(filename) + ":" + std::to_string(linenum));
    m_logFile << "[" << PAD(GetPid(), TID_LEN, ' ') << "]["
              << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
              << PAD(get_ms(cur), 3, '0') << "]["
              << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "]["
              << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg << endl
              << flush;
  } else {
    auto cur = chrono::system_clock::now();
    auto cur_time_t = chrono::system_clock::to_time_t(cur);
    auto file_and_line =
        std::string(std::string(filename) + ":" + std::to_string(linenum));
    cout << "[" << PAD(GetPid(), TID_LEN, ' ') << "]["
         << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
         << PAD(get_ms(cur), 3, '0') << "]["
         << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "]["
         << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg << endl
         << flush;
  }
}

void Logger::LogEpoch([[gnu::unused]] LEVELS level, const char* msg,
                      const char* epoch, const unsigned int linenum,
                      const char* filename, const char* function) {
  lock_guard<mutex> guard(m);

  if (m_logToFile) {
    checkLog();
    auto cur = chrono::system_clock::now();
    auto cur_time_t = chrono::system_clock::to_time_t(cur);
    auto file_and_line =
        std::string(std::string(filename) + ":" + std::to_string(linenum));
    m_logFile << "[" << PAD(GetPid(), TID_LEN, ' ') << "]["
              << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
              << PAD(get_ms(cur), 3, '0') << "]["
              << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "]["
              << LIMIT(function, MAX_FUNCNAME_LEN) << "] [Epoch " << epoch
              << "] " << msg << endl
              << flush;
  } else {
    auto cur = chrono::system_clock::now();
    auto cur_time_t = chrono::system_clock::to_time_t(cur);
    auto file_and_line =
        std::string(std::string(filename) + ":" + std::to_string(linenum));
    cout << "[" << PAD(GetPid(), TID_LEN, ' ') << "]["
         << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
         << PAD(get_ms(cur), 3, '0') << "]["
         << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "]["
         << LIMIT(function, MAX_FUNCNAME_LEN) << "] [Epoch " << epoch << "] "
         << msg << endl
         << flush;
  }
}

void Logger::LogPayload([[gnu::unused]] LEVELS level, const char* msg,
                        const bytes& payload, size_t max_bytes_to_display,
                        const unsigned int linenum, const char* filename,
                        const char* function) {
  std::unique_ptr<char[]> payload_string;
  GetPayloadS(payload, max_bytes_to_display, payload_string);
  lock_guard<mutex> guard(m);

  if (m_logToFile) {
    checkLog();
    auto cur = chrono::system_clock::now();
    auto cur_time_t = chrono::system_clock::to_time_t(cur);
    auto file_and_line =
        std::string(std::string(filename) + ":" + std::to_string(linenum));

    if (payload.size() > max_bytes_to_display) {
      m_logFile << "[" << PAD(GetPid(), TID_LEN, ' ') << "]["
                << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
                << PAD(get_ms(cur), 3, '0') << "]["
                << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN)
                << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg
                << " (Len=" << payload.size() << "): " << payload_string.get()
                << "..." << endl
                << flush;
    } else {
      m_logFile << "[" << PAD(GetPid(), TID_LEN, ' ') << "]["
                << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
                << PAD(get_ms(cur), 3, '0') << "]["
                << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN)
                << "][" << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg
                << " (Len=" << payload.size() << "): " << payload_string.get()
                << endl
                << flush;
    }
  } else {
    auto cur = chrono::system_clock::now();
    auto cur_time_t = chrono::system_clock::to_time_t(cur);
    auto file_and_line =
        std::string(std::string(filename) + ":" + std::to_string(linenum));

    if (payload.size() > max_bytes_to_display) {
      cout << "[" << PAD(GetPid(), TID_LEN, ' ') << "]["
           << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
           << PAD(get_ms(cur), 3, '0') << "]["
           << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "]["
           << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg
           << " (Len=" << payload.size() << "): " << payload_string.get()
           << "..." << endl
           << flush;
    } else {
      cout << "[" << PAD(GetPid(), TID_LEN, ' ') << "]["
           << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
           << PAD(get_ms(cur), 3, '0') << "]["
           << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "]["
           << LIMIT(function, MAX_FUNCNAME_LEN) << "] " << msg
           << " (Len=" << payload.size() << "): " << payload_string.get()
           << endl
           << flush;
    }
  }
}

void Logger::LogEpochInfo(const char* msg, const unsigned int linenum,
                          const char* filename, const char* function,
                          const char* epoch) {
  pid_t tid = getCurrentPid();
  lock_guard<mutex> guard(m);

  if (m_logToFile) {
    checkLog();
    auto cur = chrono::system_clock::now();
    auto cur_time_t = chrono::system_clock::to_time_t(cur);
    auto file_and_line =
        std::string(std::string(filename) + ":" + std::to_string(linenum));
    m_logFile << "[" << PAD(tid, TID_LEN, ' ') << "]["
              << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
              << PAD(get_ms(cur), 3, '0') << "]["
              << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "]["
              << LIMIT(function, MAX_FUNCNAME_LEN) << "] [Epoch " << epoch
              << "] " << msg << endl
              << flush;
  } else {
    auto cur = chrono::system_clock::now();
    auto cur_time_t = chrono::system_clock::to_time_t(cur);
    auto file_and_line =
        std::string(std::string(filename) + ":" + std::to_string(linenum));
    cout << "[" << PAD(tid, TID_LEN, ' ') << "]["
         << put_time(gmtime(&cur_time_t), "%y-%m-%dT%T.")
         << PAD(get_ms(cur), 3, '0') << "]["
         << LIMIT_RIGHT(file_and_line, Logger::MAX_FILEANDLINE_LEN) << "]["
         << LIMIT(function, MAX_FUNCNAME_LEN) << "] [Epoch " << epoch << "] "
         << msg << endl
         << flush;
  }
}

void Logger::DisplayLevelAbove(LEVELS level) {
  if (level != INFO && level != WARNING && level != FATAL) return;

  g3::log_levels::setHighest(level);
}

void Logger::EnableLevel(LEVELS level) { g3::log_levels::enable(level); }

void Logger::DisableLevel(LEVELS level) { g3::log_levels::disable(level); }

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
  Logger& logger = Logger::GetLogger(NULL, true);
  logger.LogGeneral(INFO, "BEG", linenum, filename, function);
}

ScopeMarker::~ScopeMarker() {
  Logger& logger = Logger::GetLogger(NULL, true);
  logger.LogGeneral(INFO, "END", m_linenum, m_filename.c_str(),
                    m_function.c_str());
}
