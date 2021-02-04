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

#ifndef ZILLIQA_DAEMON_ZILLIQADAEMON_H_
#define ZILLIQA_DAEMON_ZILLIQADAEMON_H_

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class ZilliqaDaemon {
 public:
  ZilliqaDaemon(int argc, const char* argv[], std::ofstream& log);
  void MonitorProcess(const std::string& name,
                      const bool startNewByDaemon = false);
  static void LOG(std::ofstream& log, const std::string& msg);

 private:
  std::ofstream& m_log;
  std::unordered_map<std::string, std::vector<pid_t>> m_pids;
  std::unordered_map<std::string, unsigned int> m_failedMonitorProcessCount;
  std::unordered_map<pid_t, bool> m_died;
  std::string m_privKey, m_pubKey, m_ip, m_logPath, m_nodeType, m_curPath;
  int m_port, m_recovery, m_nodeIndex;
  unsigned int m_syncType;
  bool m_cseed;

  static std::string CurrentTimeStamp();
  static std::string Execute(const std::string& cmd);
  bool DownloadPersistenceFromS3();

  std::vector<pid_t> GetProcIdByName(const std::string& procName);
  void StartNewProcess(bool cleanPersistence = false);
  void StartScripts();
  void KillProcess(const std::string& procName);
  int ReadInputs(int argc, const char* argv[]);
};

#endif  // ZILLIQA_DAEMON_ZILLIQADAEMON_H_
