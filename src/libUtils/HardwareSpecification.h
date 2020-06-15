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

#ifndef ZILLIQA_SRC_LIBUTILS_HARDWARESPECIFICATION_H_
#define ZILLIQA_SRC_LIBUTILS_HARDWARESPECIFICATION_H_

#include <fstream>
#include <iostream>
#include <string>

#include "common/Constants.h"
#include "libUtils/Logger.h"

namespace {
// RAM specific
const std::string MEMORYINFO_SOURCE_FILE = "/proc/meminfo";
const std::string TOTAL_MEMORY_KEY = "MemTotal:";
const unsigned long MINIMUM_REQ_RAM = 3800000;

// CPU specific
const unsigned int MINIMUM_REQ_NUM_OF_CPU = 2;
}  // namespace

/// Utility function to check if hardware spec of node meets the minimum
/// required ones.

namespace HardwareSpecification {

template <typename T>
bool FetchValue(const std::string& sourceFile, const std::string& key,
                T& value) {
  std::string token;
  std::ifstream file(sourceFile);
  try {
    while (file >> token) {
      if (token == key) {
        // Get next token i.e. required value
        if (file >> value) {
          return true;
        } else {
          LOG_GENERAL(WARNING, "Failed to fetch value for key : " << key);
          return false;
        }
      }
      // ignore rest of the line
      file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    // Did not found the key
    LOG_GENERAL(WARNING, "Failed to fetch value for key : " << key)
  } catch (std::exception& e) {
    LOG_GENERAL(WARNING, e.what());
    LOG_GENERAL(WARNING, "ERROR: Failed to fetch value for key : " << key);
  }
  return false;
}

bool CheckMinimumRAMReq() {
  unsigned long totalMemory = 0;
  if (FetchValue(MEMORYINFO_SOURCE_FILE, TOTAL_MEMORY_KEY, totalMemory)) {
    LOG_GENERAL(DEBUG, "RAM (KBs): " << totalMemory);
    if (totalMemory >= MINIMUM_REQ_RAM) {
      return true;
    } else {
      LOG_CHECK_FAIL("Minimum RAM (KBs): ", totalMemory, MINIMUM_REQ_RAM);
      return false;
    }
  }
  return false;  // nothing found
}

bool CheckMinimumNumOfCPUCoresReq() {
  unsigned int numOfCPUs = sysconf(_SC_NPROCESSORS_ONLN);
  if (numOfCPUs >= MINIMUM_REQ_NUM_OF_CPU) {
    LOG_GENERAL(DEBUG, "CPU(s): " << numOfCPUs);
    return true;
  } else {
    LOG_CHECK_FAIL("Minimum Number of CPU(s) : ", numOfCPUs,
                   MINIMUM_REQ_NUM_OF_CPU);
    return false;
  }
  return false;  // nothing found
}

bool CheckMinimumHardwareRequired() {
  return CheckMinimumRAMReq() && CheckMinimumNumOfCPUCoresReq();
}

}  // namespace HardwareSpecification

#endif  // ZILLIQA_SRC_LIBUTILS_HARDWARESPECIFICATION_H_