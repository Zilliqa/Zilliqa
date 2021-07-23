/*
 * Copyright (C) 2021 Zilliqa
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
#include "MemoryStats.h"
#include <errno.h>
#include <cstring>
#include "libUtils/Logger.h"
#include "sys/sysinfo.h"
#include "sys/types.h"

using namespace std;
int parseLine(char* line) {
  // This assumes that a digit will be found and the line ends in " Kb".
  int i = strlen(line);
  const char* p = line;
  while (*p < '0' || *p > '9') p++;
  line[i - 3] = '\0';
  i = atoi(p);
  return i;
}

int GetProcessPhysicalMemoryStats() {  // Note: this value is in KB!
  FILE* file = fopen("/proc/self/status", "r");
  if (file == NULL) {
    LOG_GENERAL(WARNING, "Failed to open file " << std::strerror(errno));
    return -1;
  }
  int result = -1;
  char line[128];

  while (fgets(line, 128, file) != NULL) {
    if (strncmp(line, "VmRSS:", 6) == 0) {
      result = parseLine(line);
      break;
    }
  }
  fclose(file);
  return result;
}

int GetProcessVirtualMemoryStats() {  // Note: this value is in KB!
  FILE* file = fopen("/proc/self/status", "r");
  if (file == NULL) {
    LOG_GENERAL(WARNING, "Failed to open file " << std::strerror(errno));
    return -1;
  }
  int result = -1;
  char line[128];

  while (fgets(line, 128, file) != NULL) {
    if (strncmp(line, "VmSize:", 7) == 0) {
      result = parseLine(line);
      break;
    }
  }
  fclose(file);
  return result;
}

void DisplayVirtualMemoryStats() {
  struct sysinfo memInfo = {};
  sysinfo(&memInfo);
  long long totalVirtualMem = memInfo.totalram;
  totalVirtualMem += memInfo.totalswap;
  totalVirtualMem *= memInfo.mem_unit;
  long long virtualMemUsed = memInfo.totalram - memInfo.freeram;
  virtualMemUsed += memInfo.totalswap - memInfo.freeswap;
  virtualMemUsed *= memInfo.mem_unit;
  int processVirtualMemUsed = GetProcessVirtualMemoryStats();
  if (processVirtualMemUsed == -1) return;
  LOG_GENERAL(INFO, "Total VM            = " << totalVirtualMem / 1048576
                                             << " MB"
                                             << " pid=" << Logger::GetPid());
  LOG_GENERAL(INFO, "Total VM used       = " << virtualMemUsed / 1048576
                                             << " MB"
                                             << " pid=" << Logger::GetPid());
  LOG_GENERAL(INFO, "VM used by process  = " << processVirtualMemUsed / 1024
                                             << " MB"
                                             << " pid=" << Logger::GetPid());
}

int64_t DisplayPhysicalMemoryStats(const string& str, int64_t startMem) {
  struct sysinfo memInfo = {};
  sysinfo(&memInfo);
  int processPhysMemUsed = GetProcessPhysicalMemoryStats();
  if (processPhysMemUsed == -1) return -1;
  int64_t processPhysMemUsedMB = processPhysMemUsed / 1024;
  LOG_GENERAL(INFO,
              "" << str << " PM used  = " << processPhysMemUsedMB << " MB");
  int64_t diff = processPhysMemUsedMB - startMem;
  if (startMem > 0 && diff > 0) {
    LOG_GENERAL(INFO, "PM diff = " << diff << " " << str);
  }
  return processPhysMemUsedMB;
}