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
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "Logger.h"

class SysCommand {
 public:
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
};

#endif  // __SYSCOMMAND_H__
