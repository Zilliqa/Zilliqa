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

using namespace std;
namespace po = boost::program_options;

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1

const vector<string> programName = {"zilliqa"};
const string logName = "epochinfo-00001-log.txt";
const string PRIVKEY_OPT = "--privk";
const string PUBKEY_OPT = "--pubk";
const string IP_OPT = "--address";
const string PORT_OPT = "--port";
const string SYNCTYPE_OPT = "--synctype";
const string LOGPATH_OPT = "--logpath";

string m_privKey;
string m_pubKey;
string m_ip;
string m_logPath(boost::filesystem::absolute("./").string());
int m_port = -1;
unsigned int m_syncType = 0;
const char* synctype_descr =
    "0(default) for no, 1 for new, 2 for normal, 3 for ds, 4 for lookup, 5 "
    "for node recovery, 6 for new lookup , 7 for ds guard node sync and 8 "
    "for offline validation of DB";
int m_recovery;
bool m_cseed = false;
const string launch_zilliqa = "python /zilliqa/tests/Zilliqa/launch_zilliqa.py";
const string SUSPEND_LAUNCH = "/run/zilliqa/SUSPEND_LAUNCH";
const string start_downloadScript = "python /run/zilliqa/downloadIncrDB.py";

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

string currentTimeStamp() {
  auto tm = chrono::system_clock::to_time_t(chrono::system_clock::now());
  char* t = ctime(&tm);
  if (t[strlen(t) - 1] == '\n') {
    t[strlen(t) - 1] = '\0';
  }
  return "[" + string(t) + "] : ";
}

string ReadLastLine(const string& filePath, [[gnu::unused]] ofstream& log) {
  ifstream logFile;

  logFile.open(filePath + "/" + logName);

  if (logFile.is_open()) {
    logFile.seekg(-1, ios_base::end);
  } else {
    return "";
  }

  bool keepLooping = true;

  while (keepLooping) {
    char ch;
    logFile.get(ch);

    if ((int)logFile.tellg() <= 1) {
      logFile.seekg(0);
      keepLooping = false;
    } else if (ch == ']') {
      keepLooping = false;
    } else {
      logFile.seekg(-2, ios_base::cur);
    }
  }

  string lastLine = "";
  getline(logFile, lastLine);

  logFile.close();

  return lastLine;
}

vector<pid_t> getProcIdByName(const string& procName, ofstream& log) {
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
              log << currentTimeStamp().c_str()
                  << "Failed to parse abnormal command: " << fullLine << endl;
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

static string execute(const string& cmd) {
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

static bool DownloadPersistenceFromS3(ofstream& log) {
  string output;
  log << currentTimeStamp().c_str() << "downloading persistence from S3"
      << endl;
  output = execute(start_downloadScript);
  return (output.find("Done!") != std::string::npos);
}

static void StartNewProcess(ofstream& log) {
  log << currentTimeStamp().c_str() << "Create new Zilliqa process..." << endl;
  signal(SIGCHLD, SIG_IGN);
  pid_t pid;

  if (0 == (pid = fork())) {
    bool bSuspend = false;
    while (ifstream(SUSPEND_LAUNCH).good()) {
      if (!bSuspend) {
        log << currentTimeStamp().c_str()
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
      while (!DownloadPersistenceFromS3(log)) {
        log << currentTimeStamp().c_str()
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
      log << "Suspend launch is " << bSuspend
          << ", set syncType = " << strSyncType << ", recovery = " << m_recovery
          << endl;
    }

    auto cmdToRun = launch_zilliqa + " " + m_pubKey + " " + m_privKey + " " +
                    m_ip + " " + std::to_string(m_port) + " " + strSyncType +
                    " " + m_logPath + " " + std::to_string(m_recovery);
    log << "Start to run command: \"" << cmdToRun << "\"" << endl;
    log << "\" " << execute(cmdToRun + " 2>&1") << " \"" << endl;
    exit(0);
  }
}

void MonitorProcess(unordered_map<string, vector<pid_t>>& pids,
                    unordered_map<pid_t, bool>& died, ofstream& log) {
  const string name = programName[0];

  if (pids[name].empty()) {
    log << currentTimeStamp().c_str() << "Looking for new " << name
        << " process..." << endl;
    vector<pid_t> tmp = getProcIdByName(name, log);

    for (const pid_t& i : tmp) {
      died[i] = false;
      pids[name].push_back(i);
      log << currentTimeStamp().c_str() << "Started monitoring new process "
          << name << " with PiD: " << i << endl;
    }

    return;
  }

  for (const pid_t& pid : pids[name]) {
    // If sig is 0 (the null signal), error checking is performed but no signal
    // is actually sent
    int w = kill(pid, 0);

    if (w < 0) {
      if (errno == EPERM) {
        log << currentTimeStamp().c_str() << "Daemon does not have permission "
            << "Name: " << name << " Id: " << pid << endl;
      } else if (errno == ESRCH) {
        log << currentTimeStamp().c_str() << "Process died "
            << "Name: " << name << " Id: " << pid << endl;
        died[pid] = true;
      } else {
        log << currentTimeStamp().c_str() << "Kill failed due to " << errno
            << " Name: " << name << "Id: " << pid << endl;
      }
    }

    if (died[pid]) {
      auto it = find(pids[name].begin(), pids[name].end(), pid);
      if (it != pids[name].end()) {
        log << currentTimeStamp().c_str() << "Not monitoring " << pid << " of "
            << name << endl;
        pids[name].erase(it);
      }

      StartNewProcess(log);
      died.erase(pid);
    }
  }
}

int readInputs(int argc, const char* argv[]) {
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
      "recovery,r", "Runs in recovery mode if set")(
      "logpath,g", po::value<string>(&m_logPath),
      "customized log path, could be relative path (e.g., \"./logs/\"), or "
      "absolute path (e.g., \"/usr/local/test/logs/\")");

  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);

    /** --help option
     */
    if (vm.count("help")) {
      cout << desc << endl;
      return SUCCESS;
    }

    po::notify(vm);
    m_recovery = vm.count("recovery");

    if (vm.count("cseed")) {
      cout << "Running Daemon for community seed node" << endl;
      m_cseed = true;
    }
  } catch (boost::program_options::required_option& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    std::cout << desc;
    return ERROR_IN_COMMAND_LINE;
  } catch (boost::program_options::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    return ERROR_IN_COMMAND_LINE;
  }

  return SUCCESS;
}

int main(int argc, const char* argv[]) {
  if (readInputs(argc, argv) != SUCCESS) {
    std::cout << "Failed to read inputs" << std::endl;
    exit(EXIT_FAILURE);
  }

  ofstream log;
  log.open("daemon-log.txt", fstream::out | fstream::trunc);
  for (int i = 0; i < argc; ++i) {
    log << argv[i] << " ";
  }
  log << endl;

  auto pid_parent = fork();

  if (pid_parent < 0) {
    log << "Failed to fork " << endl;
    exit(EXIT_FAILURE);
  }

  if (pid_parent > 0) {
    log << "Started daemon successfully" << endl;
    exit(EXIT_SUCCESS);
  }

  umask(0);

  auto sid = setsid();

  if (sid < 0) {
    log << "Unable to set sid" << endl;
    exit(EXIT_FAILURE);
  }

  if ((chdir("..")) < 0) {
    log << "Failed to chdir" << endl;
    exit(EXIT_FAILURE);
  }

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  StartNewProcess(log);

  unordered_map<string, vector<pid_t>> pids;
  unordered_map<pid_t, bool> died;

  while (1) {
    MonitorProcess(pids, died, log);
    sleep(5);
  }

  exit(EXIT_SUCCESS);
}
