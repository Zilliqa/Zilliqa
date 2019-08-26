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

const string launch_zilliqa = "python /zilliqa/tests/Zilliqa/launch_zilliqa.py";
const string SUSPEND_LAUNCH = "/run/zilliqa/SUSPEND_LAUNCH";
const string start_downloadScript = "python /run/zilliqa/downloadIncrDB.py";
const string default_logPath = "/run/zilliqa/";
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
  DB_VERIF
};

ZilliqaDaemon::ZilliqaDaemon(int argc, const char* argv[], std::ofstream* log)
    : m_logPath(default_logPath), m_port(-1), m_syncType(0), m_cseed(false) {
  m_log = log;

  if (ReadInputs(argc, argv) != SUCCESS) {
    *m_log << "Failed to read inputs" << endl;
    exit(EXIT_FAILURE);
  }

  *m_log << argv[0];

  for (int i = 1; i < argc; ++i) {
    *m_log << " " << argv[i];
  }

  *m_log << endl;
  KillProcess();
  StartNewProcess();
  StartScripts();
}

void ZilliqaDaemon::MonitorProcess() {
  const string name = programName[0];

  if (m_pids[name].empty()) {
    *m_log << ZilliqaDaemon::CurrentTimeStamp().c_str() << "Looking for new "
           << name << " process..." << endl;
    vector<pid_t> tmp = ZilliqaDaemon::GetProcIdByName(name);

    for (const pid_t& i : tmp) {
      m_died[i] = false;
      m_pids[name].push_back(i);
      *m_log << ZilliqaDaemon::CurrentTimeStamp().c_str()
             << "Started monitoring new process " << name << " with PiD: " << i
             << endl;
    }

    return;
  }

  for (const pid_t& pid : m_pids[name]) {
    // If sig is 0 (the null signal), error checking is performed but no signal
    // is actually sent
    int w = kill(pid, 0);

    if (w < 0) {
      if (errno == EPERM) {
        *m_log << ZilliqaDaemon::CurrentTimeStamp().c_str()
               << "Daemon does not have permission "
               << "Name: " << name << " Id: " << pid << endl;
      } else if (errno == ESRCH) {
        *m_log << ZilliqaDaemon::CurrentTimeStamp().c_str() << "Process died "
               << "Name: " << name << " Id: " << pid << endl;
        m_died[pid] = true;
      } else {
        *m_log << ZilliqaDaemon::CurrentTimeStamp().c_str()
               << "Kill failed due to " << errno << " Name: " << name
               << "Id: " << pid << endl;
      }
    }

    if (m_died[pid]) {
      auto it = find(m_pids[name].begin(), m_pids[name].end(), pid);

      if (it != m_pids[name].end()) {
        *m_log << ZilliqaDaemon::CurrentTimeStamp().c_str() << "Not monitoring "
               << pid << " of " << name << endl;
        m_pids[name].erase(it);
      }

      StartNewProcess();
      m_died.erase(pid);
    }
  }
}

string ZilliqaDaemon::CurrentTimeStamp() {
  auto tm = chrono::system_clock::to_time_t(chrono::system_clock::now());
  char* t = ctime(&tm);

  if (t[strlen(t) - 1] == '\n') {
    t[strlen(t) - 1] = '\0';
  }

  return "[" + string(t) + "] : ";
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

bool ZilliqaDaemon::DownloadPersistenceFromS3(ofstream* log) {
  string output;
  *log << ZilliqaDaemon::CurrentTimeStamp().c_str()
       << "downloading persistence from S3" << endl;
  output = Execute(start_downloadScript);
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
  *m_log << ZilliqaDaemon::CurrentTimeStamp().c_str()
         << "Create new Zilliqa process..." << endl;
  signal(SIGCHLD, SIG_IGN);

  if (0 == fork()) {
    bool bSuspend = false;

    while (ifstream(SUSPEND_LAUNCH).good()) {
      if (!bSuspend) {
        *m_log << ZilliqaDaemon::CurrentTimeStamp().c_str()
               << "Temporarily suspend launch new zilliqa process, please wait "
                  "until \""
               << SUSPEND_LAUNCH << "\" file disappeared." << endl;
        bSuspend = true;
      }

      sleep(1);
    }

    string strSyncType;

    if (m_cseed) {
      // 1. Download Incremental DB Persistence
      // 2. Restart zilliqa with syncType 6
      while (!ZilliqaDaemon::DownloadPersistenceFromS3(m_log)) {
        *m_log << ZilliqaDaemon::CurrentTimeStamp().c_str()
               << "Downloading persistence from S3 has failed. Will try again!"
               << endl;
        this_thread::sleep_for(chrono::seconds(10));
      }

      strSyncType = std::to_string(NEW_LOOKUP_SYNC);
    } else {
      /// For recover-all scenario, a SUSPEND_LAUNCH file wil be created prior
      /// to Zilliqa process being killed. Thus, we can use the variable
      /// 'bSuspend' to distinguish syncType as RECOVERY_ALL_SYNC or NO_SYNC.
      strSyncType =
          bSuspend ? to_string(RECOVERY_ALL_SYNC) : to_string(m_syncType);
      m_recovery =
          (strSyncType == to_string(RECOVERY_ALL_SYNC)) ? 1 : m_recovery;
      *m_log << "Suspend launch is " << bSuspend
             << ", set syncType = " << strSyncType
             << ", recovery = " << m_recovery << endl;
    }

    auto cmdToRun = launch_zilliqa + " " + m_pubKey + " " + m_privKey + " " +
                    m_ip + " " + std::to_string(m_port) + " " + strSyncType +
                    " " + m_logPath + " " + std::to_string(m_recovery);
    *m_log << "Start to run command: \"" << cmdToRun << "\"" << endl;
    *m_log << "\" " << Execute(cmdToRun + " 2>&1") << " \"" << endl;
    exit(0);
  }
}

void ZilliqaDaemon::StartScripts() {
  signal(SIGCHLD, SIG_IGN);

  if (m_nodeType == "lookup" && 0 == m_nodeIndex) {
    if (0 == fork()) {
      string cmdToRun = "python3 /run/zilliqa/uploadIncrDB.py &";
      *m_log << "Start to run command: \"" << cmdToRun << "\"" << endl;
      *m_log << "\" " << Execute(cmdToRun) << " \"" << endl;
      exit(0);
    }

    if (0 == fork()) {
      sleep(60);
      string cmdToRun = "python3 /run/zilliqa/auto_back_up.py -f 10 &";
      *m_log << "Start to run command: \"" << cmdToRun << "\"" << endl;
      *m_log << "\" " << Execute(cmdToRun) << " \"" << endl;
      exit(0);
    }
  }

  if (m_nodeType == "lookup" && 1 == m_nodeIndex) {
    if (0 == fork()) {
      string cmdToRun = "python3 /run/zilliqa/uploadIncrDB.py --backup &";
      *m_log << "Start to run command: \"" << cmdToRun << "\"" << endl;
      *m_log << "\" " << Execute(cmdToRun) << " \"" << endl;
      exit(0);
    }
  }
}

void ZilliqaDaemon::KillProcess() {
  const string name = programName[0];
  vector<pid_t> pids = ZilliqaDaemon::GetProcIdByName(name);

  for (const auto& pid : pids) {
    *m_log << ZilliqaDaemon::CurrentTimeStamp().c_str() << "Killing " << name
           << " process before launching daemon..." << endl;
    kill(pid, SIGTERM);
    *m_log << ZilliqaDaemon::CurrentTimeStamp().c_str() << name
           << " process killed successfully." << endl;
  }

  m_syncType = 0;
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
      "absolute path (e.g., \"/usr/local/test/logs/\")");

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);

    /** --help option
     */
    if (vm.count("help")) {
      *m_log << desc << endl;
      return SUCCESS;
    }

    po::notify(vm);
    m_recovery = vm.count("recovery");

    if (vm.count("cseed")) {
      *m_log << "Running Daemon for community seed node" << endl;
      m_cseed = true;
    }
  } catch (boost::program_options::required_option& e) {
    *m_log << "ERROR: " << e.what() << endl << endl;
    *m_log << desc;
    return ERROR_IN_COMMAND_LINE;
  } catch (boost::program_options::error& e) {
    *m_log << "ERROR: " << e.what() << endl << endl;
    return ERROR_IN_COMMAND_LINE;
  }

  return SUCCESS;
}

int main(int argc, const char* argv[]) {
  ofstream log;
  log.open(daemon_log.c_str(), fstream::out | fstream::trunc);
  pid_t pid_parent = fork();

  if (pid_parent < 0) {
    log << "Failed to fork " << endl;
    exit(EXIT_FAILURE);
  }

  if (pid_parent > 0) {
    log << "Started daemon successfully" << endl;
    exit(EXIT_SUCCESS);
  }

  umask(0);

  if (setsid() < 0) {
    log << "Unable to set sid" << endl;
    exit(EXIT_FAILURE);
  }

  if ((chdir("..")) < 0) {
    exit(EXIT_FAILURE);
  }

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  ZilliqaDaemon daemon(argc, argv, &log);

  while (1) {
    daemon.MonitorProcess();
    sleep(5);
  }

  exit(EXIT_SUCCESS);
}
