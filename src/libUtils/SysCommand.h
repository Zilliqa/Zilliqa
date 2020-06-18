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

#ifndef ZILLIQA_SRC_LIBUTILS_SYSCOMMAND_H_
#define ZILLIQA_SRC_LIBUTILS_SYSCOMMAND_H_

#include <errno.h>
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
#include <utility>

#include "Logger.h"

#define READ 0
#define WRITE 1

static std::string cmd_output_place_holder;
static int pid_place_holder;

class SysCommand {
 public:
  static FILE* popen_with_pid(const std::string& command,
                              const std::string& type, int& pid,
                              const std::string& cwd = "") {
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
      if (!cwd.empty()) {
        if (chdir(cwd.c_str()) < 0) {
          LOG_GENERAL(WARNING, "chdir failed");
          exit(1);
        }
      }
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

  static bool ExecuteCmdWithoutOutput(const std::string& cmd,
                                      const std::string& cwd = "") {
    std::string str;
    return ExecuteCmdWithOutput(cmd, str, cwd);
  }

  static bool ExecuteCmdWithOutput(std::string cmd, std::string& output,
                                   const std::string& cwd = "") {
    LOG_MARKER();

    if (!cwd.empty()) cmd = "cd " + cwd + "; " + cmd;

    std::array<char, 128> buffer{};

    signal(SIGCHLD, SIG_IGN);

    // Log the stderr into stdout as well
    cmd += " 2>&1 ";
    LOG_GENERAL(INFO, "cmd: " << cmd);
    std::unique_ptr<FILE, decltype(&pclose)> proc(popen(cmd.c_str(), "r"),
                                                  pclose);
    if (!proc) {
      LOG_GENERAL(WARNING, "popen() failed for command: " << cmd << ", Error: "
                                                          << strerror(errno));
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
                                      int& pid, const std::string& cwd = "") {
    LOG_MARKER();

    std::array<char, 128> buffer{};

    signal(SIGCHLD, SIG_IGN);

    // Log the stderr into stdout as well
    cmd += " 2>&1 ";

    std::unique_ptr<FILE, std::function<void(FILE*)>> proc(
        popen_with_pid(cmd.c_str(), "r", pid, cwd),
        [pid](FILE* ptr) { popen_with_pid(ptr, pid); });

    LOG_GENERAL(INFO, "ExecuteCmdWithOutputPID pid: " << pid);

    if (!proc) {
      LOG_GENERAL(WARNING, "popen() failed for command: " << cmd << ", Error: "
                                                          << strerror(errno));
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
                         int& pid = pid_place_holder,
                         const std::string& cwd = "") {
    switch (option) {
      case WITHOUT_OUTPUT:
        return ExecuteCmdWithoutOutput(cmd, cwd);
      case WITH_OUTPUT:
        return ExecuteCmdWithOutput(cmd, output, cwd);
      case WITH_OUTPUT_PID:
        return ExecuteCmdWithOutputPID(cmd, output, pid, cwd);
      default:
        return false;
    }
  }
};

#endif  // ZILLIQA_SRC_LIBUTILS_SYSCOMMAND_H_
