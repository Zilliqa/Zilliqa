/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
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
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

const vector<string> programName = {"zilliqa"};

const string restart_zilliqa =
    "python /zilliqa/tests/Zilliqa/daemon_restart.py";

const string proj_dir = "~/zilliqa-test";

unordered_map<int, string> PrivKey;
unordered_map<int, string> PubKey;
unordered_map<int, string> Port;
unordered_map<int, string> Path;

const string logName = "epochinfo-00001-log.txt";

static uint32_t launchDelay = 0;

enum SyncType : unsigned int {
  NO_SYNC = 0,
  NEW_SYNC,
  NORMAL_SYNC,
  DS_SYNC,
  LOOKUP_SYNC,
};

string ReadLastLine(string filePath, [[gnu::unused]] ofstream& log) {
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

SyncType getRestartValue([[gnu::unused]] pid_t pid) { return NO_SYNC; }

vector<pid_t> getProcIdByName(string procName, ofstream& log) {
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
            size_t pubkey_pos = fullLine.find('\0');
            fullLine = fullLine.substr(pubkey_pos + 1);
            size_t privkey_pos = fullLine.find('\0');
            string publicKey = fullLine.substr(0, privkey_pos);
            PubKey[id] = publicKey;

            fullLine = fullLine.substr(privkey_pos + 1);
            size_t privkey_pos_end = fullLine.find('\0');
            string privKey = fullLine.substr(0, privkey_pos_end);

            PrivKey[id] = privKey;

            fullLine = fullLine.substr(privkey_pos_end + 1);
            size_t ip_end = fullLine.find('\0');
            string ip = fullLine.substr(0, ip_end);

            fullLine = fullLine.substr(ip_end + 1);
            size_t port_end = fullLine.find('\0');
            string port = fullLine.substr(0, port_end);
            Port[id] = port;

            Path[id] = path;
            log << " id: " << id << " Path: " << path << endl;
          }
        }
      }
    }
  }

  closedir(dp);
  return result;
}
string execute(string cmd) {
  array<char, 128> buffer;
  string result;
  shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe) throw std::runtime_error("popen() failed!");
  while (!feof(pipe.get())) {
    if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
      result += buffer.data();
  }
  return result;
}

void initialize(unordered_map<string, vector<pid_t>>& pids,
                unordered_map<pid_t, bool>& died, ofstream& log) {
  bool isProcesstoTrack = false;
  for (auto v : programName) {
    vector<pid_t> tmp = getProcIdByName(v, log);
    if (tmp.size() > 0) {
      isProcesstoTrack = true;
      pids[v] = tmp;
      log << "Process " << v << " exists in " << pids[v].size() << " instances"
          << endl;
      log << "Pids: ";
      for (auto i : pids[v]) {
        log << i << " ";
        died[i] = false;
      }
      log << endl;
    } else {
      log << "Process " << v << " does not exist" << endl;
      // What to do??
    }
  }
  if (!isProcesstoTrack) {
    log << "No Process to Track\n"
        << " Exiting ..." << endl;
    exit(EXIT_SUCCESS);
  }
}

void StartNewProcess(const string pubKey, const string privKey,
                     const string port, const string syncType,
                     const string path, ofstream& log) {
  log << "Create new Zilliqa process..." << endl;
  signal(SIGCHLD, SIG_IGN);
  pid_t pid;

  if (0 == (pid = fork())) {
    log << "\" "
        << execute(restart_zilliqa + " " + pubKey + " " + privKey + " " + port +
                   " " + syncType + " " + path + " 2>&1")
        << " \"" << endl;
    exit(0);
  }
}

void MonitorProcess(unordered_map<string, vector<pid_t>>& pids,
                    unordered_map<pid_t, bool>& died, ofstream& log) {
  const string name = programName[0];

  if (pids[name].empty()) {
    log << "Looking for new " << name << " process..." << endl;
    vector<pid_t> tmp = getProcIdByName(name, log);

    for (const pid_t& i : tmp) {
      died[i] = false;
      pids[name].push_back(i);
      log << "Started monitoring new process " << name << " with PiD: " << i
          << endl;
    }

    return;
  }

  for (const pid_t& pid : pids[name]) {
    int w = kill(pid, 0);

    if (w < 0) {
      if (errno == EPERM) {
        log << "Daemon does not have permission "
            << "Name: " << name << " Id: " << pid << endl;
      } else if (errno == ESRCH) {
        log << "Process died "
            << "Name: " << name << " Id: " << pid << endl;
        died[pid] = true;
      } else {
        log << "Kill failed due to " << errno << " Name: " << name
            << "Id: " << pid << endl;
      }
    }

    if (died[pid]) {
      auto it = find(pids[name].begin(), pids[name].end(), pid);
      if (it != pids[name].end()) {
        log << "Not monitoring " << pid << " of " << name << endl;
        pids[name].erase(it);
      }

      log << "Sleep " << launchDelay
          << " seconds before re-launch a new process..." << endl;
      sleep(launchDelay);
      StartNewProcess(PubKey[pid], PrivKey[pid], Port[pid],
                      to_string(getRestartValue(pid)), Path[pid], log);
      died.erase(pid);
      PrivKey.erase(pid);
      PubKey.erase(pid);
      Port.erase(pid);
      Path.erase(pid);
    }
  }
}

int main(int argc, const char* argv[]) {
  if (argc > 1) {
    launchDelay = stoull(argv[1]);
  }

  pid_t pid_parent, sid;
  ofstream log;
  log.open("daemon-log.txt", fstream::out | fstream::trunc);

  pid_parent = fork();

  if (pid_parent < 0) {
    log << "Failed to fork " << endl;
    exit(EXIT_FAILURE);
  }

  if (pid_parent > 0) {
    log << "Started daemon successfully" << endl;
    exit(EXIT_SUCCESS);
  }

  umask(0);

  sid = setsid();

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

  unordered_map<string, vector<pid_t>> pids;
  unordered_map<pid_t, bool> died;

  initialize(pids, died, log);

  while (1) {
    MonitorProcess(pids, died, log);
    sleep(5);
  }

  exit(EXIT_SUCCESS);
}
