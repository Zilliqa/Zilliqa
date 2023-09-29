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

#include "ZilliqaDaemon.h"
#include "ZilliqaUpdater.h"

#include "common/Constants.h"
#include "libUtils/Logger.h"

#include <boost/program_options.hpp>

#include <dirent.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace std;
namespace po = boost::program_options;

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1

#define MONITORING_FAIL_COUNT 10

// prevent collision of our logger's macro
#ifdef LOG
#undef LOG
#endif

const vector<string> programName = {"zilliqa"};
const string SYNCTYPE_OPT = "--synctype";
const char* synctype_descr =
    "0(default) for no, 1 for new, 2 for normal, 3 for ds, 4 for lookup, 5 "
    "for node recovery, 6 for new lookup , 7 for ds guard node sync and 8 "
    "for offline validation of DB";

const string SUSPEND_LAUNCH = "SUSPEND_LAUNCH";
const string upload_incr_DB_script = "upload_incr_DB.py";
const string download_incr_DB_script = "download_incr_DB.py";
const string auto_backup_script = "auto_backup.py";
const string daemon_log = "daemon-log.txt";

ZilliqaDaemon::~ZilliqaDaemon() noexcept {
  if (m_updater) m_updater->Stop();
}

ZilliqaDaemon::ZilliqaDaemon(int argc, const char* argv[], std::ofstream& log)
    : m_log(log),
      m_logPath(std::filesystem::current_path().string() + "/"),
      m_curPath(std::filesystem::current_path().string() + "/"),
      m_port(-1),
      m_recovery(0),
      m_nodeIndex(0),
      m_syncType(0),
      m_cseed(false),
      m_kill(true) {
  if (ReadInputs(argc, argv) != SUCCESS) {
    ZilliqaDaemon::LOG(m_log, "Failed to read inputs.");
    exit(EXIT_FAILURE);
  }

  string msg = argv[0];

  for (int i = 1; i < argc; ++i) {
    msg += string(" ") + argv[i];
  }

  if (AUTO_UPGRADE) {
    m_updater =
        std::make_unique<ZilliqaUpdater>([this](const std::string& procName) {
          return GetMonitoredProcIdsByName(procName);
        });
    m_updater->Start();
  }

  ZilliqaDaemon::LOG(m_log, msg);
  StartNewProcess();
}

void ZilliqaDaemon::MonitorProcess(const string& name,
                                   const bool startNewByDaemon) {
  bool noPids = false;

  // IMPORTANT: we only lock at specific sites because StartNewProcess() calls
  //            fork() and we don't want the locks to be copied as part of this
  //            which could lead to deadlocks.
  {
    std::shared_lock<std::shared_mutex> guard{m_mutex};
    const auto iter = m_pids.find(name);
    noPids = iter == std::end(m_pids) || iter->second.empty();
  }

  if (noPids) {
    ZilliqaDaemon::LOG(m_log, "Looking for new " + name + " process...");
    vector<pid_t> tmp = ZilliqaDaemon::GetProcIdByName(name);
    if (tmp.empty()) {
      if (!startNewByDaemon &&
          (m_nodeType == "dsguard" || m_nodeType == "normal")) {
        if (++m_failedMonitorProcessCount[name] >= MONITORING_FAIL_COUNT) {
          StartNewProcess(true);
          m_failedMonitorProcessCount[name] = 0;
        }
      }
    }

    std::unique_lock<std::shared_mutex> guard{m_mutex};
    for (auto pid : tmp) {
      m_died[pid] = false;
      m_pids[name].push_back(pid);
      ZilliqaDaemon::LOG(m_log, "Started monitoring new process " + name +
                                    " with PiD: " + to_string(pid));
    }
    return;
  }

  std::vector<pid_t> pids;
  {
    std::shared_lock<std::shared_mutex> guard{m_mutex};
    const auto iter = m_pids.find(name);
    if (iter != std::end(m_pids)) pids = iter->second;
  }

  for (const pid_t& pid : pids) {
    // If sig is 0 (the null signal), error checking is performed but no signal
    // is actually sent
    if (m_kill and kill(pid, 0) < 0) {
      if (errno == EPERM) {
        ZilliqaDaemon::LOG(m_log, "Daemon does not have permission Name: " +
                                      name + " Id: " + to_string(pid));
      } else if (errno == ESRCH) {
        ZilliqaDaemon::LOG(m_log, "We think Process died Name: " + name +
                                      " Id: " + to_string(pid));
        m_died[pid] = true;
      } else {
        ZilliqaDaemon::LOG(m_log, "Kill failed due to " + to_string(errno) +
                                      " Name: " + name +
                                      "Id: " + to_string(pid));
      }
    }

    if (m_died[pid]) {
      {
        std::unique_lock<std::shared_mutex> guard{m_mutex};
        auto it = find(m_pids[name].begin(), m_pids[name].end(), pid);

        if (it != m_pids[name].end()) {
          ZilliqaDaemon::LOG(
              m_log, "Not monitoring " + to_string(pid) + " of " + name);
          m_pids[name].erase(it);
        }
      }

      bool toCleanPersistence =
          (m_nodeType == "dsguard" || m_nodeType == "normal");
      StartNewProcess(toCleanPersistence);
      m_died.erase(pid);
    }
  }
}

void ZilliqaDaemon::LOG(ofstream& log, const string& msg) {
  log << "[" << ZilliqaDaemon::CurrentTimeStamp() << "] : " << msg << endl;
}

string ZilliqaDaemon::CurrentTimeStamp() {
  auto tm = chrono::system_clock::to_time_t(chrono::system_clock::now());
  char mbstr[100];
  size_t size = strftime(mbstr, sizeof(mbstr), "%c", localtime(&tm));
  mbstr[size] = '\0';
  return mbstr;
}

string ZilliqaDaemon::Execute(const string& cmd) {
  array<char, 128> buffer{};
  string result;
  shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);

  if (!pipe) throw std::runtime_error("popen() failed!");

  while (!feof(pipe.get())) {
    if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
      result += buffer.data();
  }

  return result;
}

bool ZilliqaDaemon::DownloadPersistenceFromS3() {
  string output;
  ZilliqaDaemon::LOG(m_log, "downloading persistence from S3.");
  output = Execute("python3 " + m_curPath + download_incr_DB_script);
  return (output.find("Done!") != std::string::npos);
}

vector<pid_t> ZilliqaDaemon::GetProcIdByName(const string& procName) {
  vector<pid_t> result;
  result.clear();

  // Open the /proc directory
  auto closeDir = [](DIR* dir) {
    if (dir) closedir(dir);
  };
  std::unique_ptr<DIR, decltype(closeDir)> dp{opendir("/proc"), closeDir};

  if (dp) {
    // Enumerate all entries in directory until process found
    struct dirent* dirp;

    while ((dirp = readdir(dp.get()))) {
      // Skip non-numeric entries
      int id = atoi(dirp->d_name);

      if (id > 0) {
        // Read contents of virtual /proc/{pid}/cmdline file
        string cmdPath = string("/proc/") + dirp->d_name + "/cmdline";
        ifstream cmdFile(cmdPath.c_str());
        string cmdLine;
        string fullLine;
        getline(cmdFile, fullLine);

        if (!fullLine.empty()) {
          // Keep first cmdline item which contains the program path
          size_t pos = fullLine.find('\0');

          if (pos != string::npos) cmdLine = fullLine.substr(0, pos);
          // Keep program name only, removing the path
          pos = cmdLine.rfind('/');
          string path = "";

          if (pos != string::npos) {
            path = fullLine.substr(0, pos);
            cmdLine = cmdLine.substr(pos + 1);
            fullLine = fullLine.substr(pos + 1);
          }

          // Compare against requested process name
          if (procName == cmdLine) {
            result.push_back(id);
            size_t space_pos = fullLine.find('\0');

            if (string::npos == space_pos) {
              return result;
            }

            fullLine = fullLine.substr(space_pos + 1);

            while (!fullLine.empty() &&
                   (string::npos != (space_pos = fullLine.find('\0')))) {
              string token = fullLine.substr(0, space_pos);
              fullLine = fullLine.substr(space_pos + 1);

              if (token == SYNCTYPE_OPT) {
                space_pos = (string::npos == fullLine.find('\0'))
                                ? fullLine.size()
                                : fullLine.find('\0');
                m_syncType = stoi(fullLine.substr(0, space_pos));
                fullLine = fullLine.substr(space_pos + 1);
                continue;
              }
            }
          }
        }
      }
    }
  }

  return result;
}

void ZilliqaDaemon::StartNewProcess(bool cleanPersistence) {
  // Kill running processes ( zilliqa & scilla) if any
  KillProcess(programName[0]);
  KillProcess("scilla-server");
  KillProcess("evm-ds");

  bool updating = m_updater && m_updater->Updating();

  ZilliqaDaemon::LOG(m_log, std::string{"Create new Zilliqa process..."} +
                                (updating ? "(updating)" : ""));
  // signal(SIGCHLD, SIG_IGN);

  bool updated = updating && m_updater->Update();

  pid_t pid_parent = fork();

  if (pid_parent < 0) {
    ZilliqaDaemon::LOG(m_log, "Failed to fork.");
    Exit(EXIT_FAILURE);
  }

  if (pid_parent > 0) {
    StartScripts();
    return;
  }

  bool bSuspend = false;

  while (ifstream(m_curPath + SUSPEND_LAUNCH).good()) {
    if (!bSuspend) {
      ZilliqaDaemon::LOG(m_log,
                         "Temporarily suspend launch new zilliqa process, "
                         "please wait until \"" +
                             SUSPEND_LAUNCH + "\" file disappeared.");
      bSuspend = true;
    }

    sleep(1);
  }

  string strSyncType;

  if (m_cseed) {
    // 1. Download Incremental DB Persistence
    // 2. Restart zilliqa with syncType 6
    while (!DownloadPersistenceFromS3()) {
      ZilliqaDaemon::LOG(
          m_log, "Downloading persistence from S3 has failed. Will try again!");
      this_thread::sleep_for(chrono::seconds(10));
    }

    strSyncType = to_string(NEW_LOOKUP_SYNC);
  } else {
    /// For recover-all scenario, a SUSPEND_LAUNCH file wil be created prior
    /// to Zilliqa process being killed. Thus, we can use the variable
    /// 'bSuspend' to distinguish syncType as RECOVERY_ALL_SYNC or NO_SYNC.
    m_syncType = ((bSuspend || cleanPersistence) && !updated)
                     ? RECOVERY_ALL_SYNC
                     : m_syncType;
    strSyncType = to_string(updated ? NORMAL_SYNC : m_syncType);
    m_recovery = m_syncType == RECOVERY_ALL_SYNC ? 1 : m_recovery;
    ZilliqaDaemon::LOG(m_log, "Suspend launch is " + to_string(bSuspend) +
                                  ", set syncType = " + strSyncType +
                                  ", recovery = " + to_string(m_recovery));
  }

  if (!bSuspend && cleanPersistence) {
    ZilliqaDaemon::LOG(m_log, "Start to run command: rm -rf persistence");
    ZilliqaDaemon::LOG(
        m_log,
        "\" " + Execute("cd " + m_curPath + "; rm -rf persistence") + " \"");
  }

  string identity = m_nodeType + "-" + std::to_string(m_nodeIndex);

  string cmdToRun = string("zilliqa") + " --privk " + m_privKey + " --pubk " +
                    m_pubKey + " --address " + m_ip + " --port " +
                    to_string(m_port) + " --synctype " + strSyncType +
                    " --logpath " + m_logPath + " --identity " + identity;

  if (1 == m_recovery) {
    if (updated)
      ZilliqaDaemon::LOG(m_log, "Not adding --recovery flag due to update");
    else
      cmdToRun += " --recovery";
  }

  ZilliqaDaemon::LOG(m_log, "Start to run command: \"" + cmdToRun + "\"");
  ZilliqaDaemon::LOG(
      m_log, "\" " +
                 Execute("cd " + m_curPath +
                         "; ulimit -Sc unlimited; ulimit -Hc unlimited;" +
                         cmdToRun + " >> ./error_log_zilliqa 2>&1") +
                 " \"");

  Exit(0);
}

void ZilliqaDaemon::StartScripts() {
  // signal(SIGCHLD, SIG_IGN);

  if (m_nodeIndex < 0 || m_nodeIndex > 1 || m_nodeType != "lookup") return;

  pid_t pid_parent = fork();
  if (pid_parent > 0) return;

  if (pid_parent < 0) {
    ZilliqaDaemon::LOG(m_log, "Failed to fork.");
    Exit(EXIT_FAILURE);
  }

  auto script = (0 == m_nodeIndex) ? upload_incr_DB_script : auto_backup_script;

  string cmdToRun = "ps axf | grep " + script +
                    " | grep -v grep  | awk '{print \"kill -9 \" $1}'| sh &";

  if (m_kill) {
    ZilliqaDaemon::LOG(m_log, "Start to run command: \"" + cmdToRun + "\"");
    ZilliqaDaemon::LOG(m_log, "\" " + Execute(cmdToRun + " 2>&1") + " \"");
  } else {
    ZilliqaDaemon::LOG(m_log, "Not running command: \"" + cmdToRun + "\"");
  }

  cmdToRun = "python3 " + m_curPath + script +
             (0 == m_nodeIndex ? "" : " -f 10") + " &";
  ZilliqaDaemon::LOG(m_log, "Start to run command: \"" + cmdToRun + "\"");
  ZilliqaDaemon::LOG(m_log, "\" " + Execute(cmdToRun + " 2>&1") + " \"");

  Exit(0);
}

void ZilliqaDaemon::Exit(int exitCode) {
  // Since the updater uses the Logger and the daemon keeps fork-ing
  // we can't realy on exit() because it will hang when the logger
  // tries to shutdown (since the child won't have the same running threads).
  if (m_updater) {
    m_log.flush();
    _exit(exitCode);
  }

  exit(exitCode);
}

void ZilliqaDaemon::KillProcess(const string& procName) {
  vector<pid_t> pids = ZilliqaDaemon::GetProcIdByName(procName);
  for (const auto& pid : pids) {
    if (m_kill) {
      ZilliqaDaemon::LOG(
          m_log, "Killing " + procName + " process before launching daemon...");

      kill(pid, SIGTERM);
      ZilliqaDaemon::LOG(m_log, procName + " process killed successfully.");
    }
  }
}

int ZilliqaDaemon::ReadInputs(int argc, const char* argv[]) {
  po::options_description desc("Options");

  desc.add_options()("help,h", "Print help messages")(
      "privk,i", po::value<string>(&m_privKey)->required(),
      "32-byte private key")("pubk,u", po::value<string>(&m_pubKey)->required(),
                             "33-byte public key")(
      "address,a", po::value<string>(&m_ip)->required(),
      "Listen IPv4/6 address formated as \"dotted decimal\" or optionally "
      "\"dotted decimal:portnumber\" format, otherwise \"NAT\"")(
      "port,p", po::value<int>(&m_port),
      "Specifies port to bind to, if not specified in address")(
      "loadconfig,l", "Loads configuration if set (deprecated)")(
      "synctype,s", po::value<unsigned int>(&m_syncType), synctype_descr)(
      "nodetype,n", po::value<string>(&m_nodeType)->required(),
      "Specifies node type")(
      "nodeindex,x", po::value<int>(&m_nodeIndex)->required(),
      "Specifies node index")("recovery,r", "Runs in recovery mode if set")(
      "logpath,g", po::value<string>(&m_logPath),
      "customized log path, could be relative path (e.g., \"./logs/\"), or "
      "absolute path (e.g., \"/usr/local/test/logs/\")")(
      "cseed,c", "Runs as cummunity seed node if set")(
      "killnone,k", "does not kill processes");

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);

    /** --help option
     */
    if (vm.count("help")) {
      m_log << desc << endl;
      return SUCCESS;
    }

    po::notify(vm);
    m_recovery = vm.count("recovery");

    if (vm.count("cseed")) {
      ZilliqaDaemon::LOG(m_log, "Running Daemon for community seed node.");
      m_cseed = true;
    }
    if (vm.count("killnone")) {
      ZilliqaDaemon::LOG(
          m_log, "does not kill things - useful for experimental native.");
      m_kill = false;
    }
  } catch (boost::program_options::required_option& e) {
    ZilliqaDaemon::LOG(m_log, "ERROR: " + string(e.what()));
    return ERROR_IN_COMMAND_LINE;
  } catch (boost::program_options::error& e) {
    ZilliqaDaemon::LOG(m_log, "ERROR: " + string(e.what()));
    return ERROR_IN_COMMAND_LINE;
  }

  return SUCCESS;
}

std::vector<pid_t> ZilliqaDaemon::GetMonitoredProcIdsByName(
    const std::string& procName) const {
  std::vector<pid_t> result;

  std::shared_lock<std::shared_mutex> guard{m_mutex};

  auto iter = m_pids.find(procName);
  if (iter != std::end(m_pids)) result = iter->second;

  return result;
}

int main(int argc, const char* argv[]) {
  ofstream log;
  log.open(daemon_log.c_str(), fstream::out | fstream::trunc);

  pid_t pid_parent = fork();

  if (pid_parent < 0) {
    ZilliqaDaemon::LOG(log, "Failed to fork.");
    exit(EXIT_FAILURE);
  }

  if (pid_parent > 0) {
    ZilliqaDaemon::LOG(log, "Started daemon successfully.");
    exit(EXIT_SUCCESS);
  }

  umask(0);

  if (setsid() < 0) {
    ZilliqaDaemon::LOG(log, "Unable to set sid.");
    exit(EXIT_FAILURE);
  }

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  ZilliqaDaemon daemon(argc, argv, log);

  bool startNewByDaemon = true;
  while (1) {
    for (const auto& name : programName) {
      std::cout << "Monitoring " << name << " process..." << std::endl;
      daemon.MonitorProcess(name, startNewByDaemon);
    }

    sleep(5);

    startNewByDaemon = false;
  }

  exit(EXIT_SUCCESS);
}
