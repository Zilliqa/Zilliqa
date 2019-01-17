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

#ifndef __SYSCOMMAND_H__
#define __SYSCOMMAND_H__

#include <signal.h>
#include <sys/wait.h>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include "Logger.h"

#define READ 0
#define WRITE 1

static std::string cmd_output_place_holder;
static int pid_place_holder;

class SysCommand {
 public:
  static FILE* popen_with_pid(std::string command, std::string type, int& pid) {
    pid_t child_pid;
    int fd[2];
    if (pipe(fd) != 0) {
      LOG_GENERAL(WARNING, "Failed to pipe fd");
    }

    if ((child_pid = fork()) == -1) {
      perror("fork");
      exit(1);
    }

    /* child process */
    if (child_pid == 0) {
      if (type == "r") {
        close(fd[READ]);  // Close the READ end of the pipe since the child's fd
                          // is write-only
        dup2(fd[WRITE], 1);  // Redirect stdout to pipe
      } else {
        close(fd[WRITE]);   // Close the WRITE end of the pipe since the child's
                            // fd is read-only
        dup2(fd[READ], 0);  // Redirect stdin to pipe
      }

      setpgid(
          child_pid,
          child_pid);  // Needed so negative PIDs can kill children of /bin/sh
      execl("/bin/sh", "/bin/sh", "-c", command.c_str(), NULL);
      exit(0);
    } else {
      if (type == "r") {
        close(fd[WRITE]);  // Close the WRITE end of the pipe since parent's fd
                           // is read-only
      } else {
        close(fd[READ]);  // Close the READ end of the pipe since parent's fd is
                          // write-only
      }
    }

    pid = child_pid;

    if (type == "r") {
      return fdopen(fd[READ], "r");
    }

    return fdopen(fd[WRITE], "w");
  }

  static int popen_with_pid(FILE* fp, int pid) {
    int stat;

    fclose(fp);
    while (waitpid((pid_t)pid, &stat, 0) == -1) {
      if (errno != EINTR) {
        stat = -1;
        break;
      }
    }

    return stat;
  }

  static bool ExecuteCmdWithoutOutput(std::string cmd) {
    std::string str;
    return ExecuteCmdWithOutput(cmd, str);
  }

  static bool ExecuteCmdWithOutput(std::string cmd, std::string& output) {
    LOG_MARKER();

    std::array<char, 128> buffer;

    signal(SIGCHLD, SIG_IGN);

    // Log the stderr into stdout as well
    cmd += " 2>&1 ";
    std::unique_ptr<FILE, decltype(&pclose)> proc(popen(cmd.c_str(), "r"),
                                                  pclose);
    if (!proc) {
      LOG_GENERAL(WARNING, "popen() failed!");
      return false;
    }

    while (!feof(proc.get())) {
      if (fgets(buffer.data(), 128, proc.get()) != nullptr) {
        output += buffer.data();
      }
    }

    return true;
  }

  static bool ExecuteCmdWithOutputPID(std::string cmd, std::string& output,
                                      int& pid) {
    LOG_MARKER();

    std::array<char, 128> buffer;

    signal(SIGCHLD, SIG_IGN);

    // Log the stderr into stdout as well
    cmd += " 2>&1 ";
    std::unique_ptr<FILE, std::function<void(FILE*)>> proc(
        popen_with_pid(cmd.c_str(), "r", pid),
        [pid](FILE* ptr) { popen_with_pid(ptr, pid); });

    LOG_GENERAL(INFO, "ExecuteCmdWithOutputPID pid: " << pid);

    if (!proc) {
      LOG_GENERAL(WARNING, "popen() failed!");
      return false;
    }

    while (!feof(proc.get())) {
      if (fgets(buffer.data(), 128, proc.get()) != nullptr) {
        output += buffer.data();
      }
    }

    return true;
  }

  enum SYSCMD_OPTION { WITHOUT_OUTPUT, WITH_OUTPUT, WITH_OUTPUT_PID };

  static bool ExecuteCmd(const SYSCMD_OPTION& option, const std::string& cmd,
                         std::string& output = cmd_output_place_holder,
                         int& pid = pid_place_holder) {
    switch (option) {
      case WITHOUT_OUTPUT:
        return ExecuteCmdWithoutOutput(cmd);
      case WITH_OUTPUT:
        return ExecuteCmdWithOutput(cmd, output);
      case WITH_OUTPUT_PID:
        return ExecuteCmdWithOutputPID(cmd, output, pid);
      default:
        return false;
    }
  }
};

#endif  // __SYSCOMMAND_H__
