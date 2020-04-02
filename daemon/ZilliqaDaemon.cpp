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

using namespace std;
namespace po = boost::program_options;

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1

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

enum SyncType : unsigned int {
  NO_SYNC = 0,
  NEW_SYNC,
  NORMAL_SYNC,
  DS_SYNC,
  LOOKUP_SYNC,
  RECOVERY_ALL_SYNC,
  NEW_LOOKUP_SYNC,
  GUARD_DS_SYNC,
  DB_VERIF  // Deprecated
};

ZilliqaDaemon::ZilliqaDaemon(int argc, const char* argv[], std::ofstream& log)
    : m_log(log),
      m_logPath(boost::filesystem::current_path().string() + "/"),
      m_curPath(boost::filesystem::current_path().string() + "/"),
      m_port(-1),
      m_recovery(0),
      m_nodeIndex(0),
      m_syncType(0),
      m_cseed(false) {
  if (ReadInputs(argc, argv) != SUCCESS) {
    ZilliqaDaemon::LOG(m_log, "Failed to read inputs.");
    exit(EXIT_FAILURE);
  }

  string msg = argv[0];

  for (int i = 1; i < argc; ++i) {
    msg += string(" ") + argv[i];
  }

  ZilliqaDaemon::LOG(m_log, msg);
  StartNewProcess();
}

void ZilliqaDaemon::MonitorProcess(const string& name) {
  if (m_pids[name].empty()) {
    ZilliqaDaemon::LOG(m_log, "Looking for new " + name + " process...");
    vector<pid_t> tmp = ZilliqaDaemon::GetProcIdByName(name);

    for (const pid_t& i : tmp) {
      m_died[i] = false;
      m_pids[name].push_back(i);
      ZilliqaDaemon::LOG(m_log, "Started monitoring new process " + name +
                                    " with PiD: " + to_string(i));
    }

    return;
  }

  for (const pid_t& pid : m_pids[name]) {
    // If sig is 0 (the null signal), error checking is performed but no signal
    // is actually sent
    int w = kill(pid, 0);

    if (w < 0) {
      if (errno == EPERM) {
        ZilliqaDaemon::LOG(m_log, "Daemon does not have permission Name: " +
                                      name + " Id: " + to_string(pid));
      } else if (errno == ESRCH) {
        ZilliqaDaemon::LOG(
            m_log, "Process died Name: " + name + " Id: " + to_string(pid));
        m_died[pid] = true;
      } else {
        ZilliqaDaemon::LOG(m_log, "Kill failed due to " + to_string(errno) +
                                      " Name: " + name +
                                      "Id: " + to_string(pid));
      }
    }

    if (m_died[pid]) {
      auto it = find(m_pids[name].begin(), m_pids[name].end(), pid);

      if (it != m_pids[name].end()) {
        ZilliqaDaemon::LOG(m_log,
                           "Not monitoring " + to_string(pid) + " of " + name);
        m_pids[name].erase(it);
      }

      StartNewProcess();
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
  output = Execute("python " + m_curPath + download_incr_DB_script);
  return (output.find("Done!") != std::string::npos);
}

vector<pid_t> ZilliqaDaemon::GetProcIdByName(const string& procName) {
  vector<pid_t> result;
  result.clear();

  // Open the /proc directory
  DIR* dp = opendir("/proc");

  if (dp != NULL) {
    // Enumerate all entries in directory until process found
    struct dirent* dirp;

    while ((dirp = readdir(dp))) {
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
              closedir(dp);
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

  closedir(dp);
  return result;
}

void ZilliqaDaemon::StartNewProcess() {
  KillProcess();

  ZilliqaDaemon::LOG(m_log, "Create new Zilliqa process...");
  signal(SIGCHLD, SIG_IGN);

  pid_t pid_parent = fork();

  if (pid_parent < 0) {
    ZilliqaDaemon::LOG(m_log, "Failed to fork.");
    exit(EXIT_FAILURE);
  }

  if (pid_parent == 0) {
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
            m_log,
            "Downloading persistence from S3 has failed. Will try again!");
        this_thread::sleep_for(chrono::seconds(10));
      }

      strSyncType = to_string(NEW_LOOKUP_SYNC);
    } else {
      /// For recover-all scenario, a SUSPEND_LAUNCH file wil be created prior
      /// to Zilliqa process being killed. Thus, we can use the variable
      /// 'bSuspend' to distinguish syncType as RECOVERY_ALL_SYNC or NO_SYNC.
      m_syncType = bSuspend ? RECOVERY_ALL_SYNC : m_syncType;
      strSyncType = to_string(m_syncType);
      m_recovery = m_syncType == RECOVERY_ALL_SYNC ? 1 : m_recovery;
      ZilliqaDaemon::LOG(m_log, "Suspend launch is " + to_string(bSuspend) +
                                    ", set syncType = " + strSyncType +
                                    ", recovery = " + to_string(m_recovery));
    }

    string cmdToRun = string("zilliqa") + " --privk " + m_privKey + " --pubk " +
                      m_pubKey + " --address " + m_ip + " --port " +
                      to_string(m_port) + " --synctype " + strSyncType +
                      " --logpath " + m_logPath;

    if (1 == m_recovery) {
      cmdToRun += " --recovery";
    }

    ZilliqaDaemon::LOG(m_log, "Start to run command: \"" + cmdToRun + "\"");
    ZilliqaDaemon::LOG(
        m_log, "\" " +
                   Execute("cd " + m_curPath +
                           "; ulimit -Sc unlimited; ulimit -Hc unlimited;" +
                           cmdToRun + " >> ./error_log_zilliqa 2>&1") +
                   " \"");
    exit(0);
  }

  StartScripts();
}

void ZilliqaDaemon::StartScripts() {
  signal(SIGCHLD, SIG_IGN);

  if (m_nodeType == "lookup" && 0 == m_nodeIndex) {
    pid_t pid_parent = fork();

    if (pid_parent < 0) {
      ZilliqaDaemon::LOG(m_log, "Failed to fork.");
      exit(EXIT_FAILURE);
    }

    if (pid_parent == 0) {
      string cmdToRun =
          "ps axf | grep " + upload_incr_DB_script +
          " | grep -v grep  | awk '{print \"kill -9 \" $1}'| sh &";
      ZilliqaDaemon::LOG(m_log, "Start to run command: \"" + cmdToRun + "\"");
      ZilliqaDaemon::LOG(m_log, "\" " + Execute(cmdToRun + " 2>&1") + " \"");
      cmdToRun = "python3 " + m_curPath + upload_incr_DB_script + " &";
      ZilliqaDaemon::LOG(m_log, "Start to run command: \"" + cmdToRun + "\"");
      ZilliqaDaemon::LOG(m_log, "\" " + Execute(cmdToRun + " 2>&1") + " \"");
      exit(0);
    }

    pid_parent = fork();

    if (pid_parent < 0) {
      ZilliqaDaemon::LOG(m_log, "Failed to fork.");
      exit(EXIT_FAILURE);
    }

    if (pid_parent == 0) {
      string cmdToRun =
          "ps axf | grep " + auto_backup_script +
          " | grep -v grep  | awk '{print \"kill -9 \" $1}'| sh &";
      ZilliqaDaemon::LOG(m_log, "Start to run command: \"" + cmdToRun + "\"");
      ZilliqaDaemon::LOG(m_log, "\" " + Execute(cmdToRun + " 2>&1") + " \"");
      cmdToRun = "python3 " + m_curPath + auto_backup_script + " -f 10 &";
      ZilliqaDaemon::LOG(m_log, "Start to run command: \"" + cmdToRun + "\"");
      ZilliqaDaemon::LOG(m_log, "\" " + Execute(cmdToRun + " 2>&1") + " \"");
      exit(0);
    }
  }

  if (m_nodeType == "lookup" && 1 == m_nodeIndex) {
    pid_t pid_parent = fork();

    if (pid_parent < 0) {
      ZilliqaDaemon::LOG(m_log, "Failed to fork.");
      exit(EXIT_FAILURE);
    }

    if (pid_parent == 0) {
      string cmdToRun =
          "ps axf | grep " + upload_incr_DB_script +
          " | grep -v grep  | awk '{print \"kill -9 \" $1}'| sh &";
      ZilliqaDaemon::LOG(m_log, "Start to run command: \"" + cmdToRun + "\"");
      ZilliqaDaemon::LOG(m_log, "\" " + Execute(cmdToRun + " 2>&1") + " \"");
      cmdToRun = "python3 " + m_curPath + upload_incr_DB_script + " --backup &";
      ZilliqaDaemon::LOG(m_log, "Start to run command: \"" + cmdToRun + "\"");
      ZilliqaDaemon::LOG(m_log, "\" " + Execute(cmdToRun + " 2>&1") + " \"");
      exit(0);
    }
  }
}

void ZilliqaDaemon::KillProcess() {
  const string name = programName[0];
  vector<pid_t> pids = ZilliqaDaemon::GetProcIdByName(name);

  for (const auto& pid : pids) {
    ZilliqaDaemon::LOG(
        m_log, "Killing " + name + " process before launching daemon...");
    kill(pid, SIGTERM);
    ZilliqaDaemon::LOG(m_log, name + " process killed successfully.");
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
      "cseed,c", "Runs as cummunity seed node if set");

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
  } catch (boost::program_options::required_option& e) {
    ZilliqaDaemon::LOG(m_log, "ERROR: " + string(e.what()));
    return ERROR_IN_COMMAND_LINE;
  } catch (boost::program_options::error& e) {
    ZilliqaDaemon::LOG(m_log, "ERROR: " + string(e.what()));
    return ERROR_IN_COMMAND_LINE;
  }

  return SUCCESS;
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

  while (1) {
    for (const auto& name : programName) {
      daemon.MonitorProcess(name);
    }

    sleep(5);
  }

  exit(EXIT_SUCCESS);
}
