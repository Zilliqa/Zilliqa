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

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/program_options.hpp>

#include "common/MessageNames.h"
#include "libUtils/SWInfo.h"

namespace po = boost::program_options;

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_UNHANDLED_EXCEPTION -2
#define ERROR_UNEXPECTED -3

struct MessageSizeTime {
  uint32_t size;
  uint32_t time;
};

using VectorOfSizeTime = std::vector<MessageSizeTime>;
using MapOfMessageSizeTime = std::map<std::string, VectorOfSizeTime>;

static MapOfMessageSizeTime resultMessageSizeTime;

bool grepFile(const std::string& strFileName) {
  std::ifstream fs(strFileName, std::ifstream::in);
  if (!fs.is_open()) {
    std::cout << "Failed to open file " + strFileName;
    return false;
  }

  std::string strLine;
  size_t pos = std::string::npos;
  std::unordered_map<std::string, uint32_t> mapMessageSize;
  while (std::getline(fs, strLine, '\n')) {
    if ((pos = strLine.find(MessageSizeKeyword)) != std::string::npos) {
      auto posMessageNameEnd =
          strLine.find(' ', pos + MessageSizeKeyword.length());
      std::string strMessageName;
      if (posMessageNameEnd != std::string::npos) {
        strMessageName = strLine.substr(
            pos + MessageSizeKeyword.length(),
            posMessageNameEnd - pos - MessageSizeKeyword.length());
      }

      std::string strSize = strLine.substr(posMessageNameEnd + 1);
      mapMessageSize[strMessageName] = std::stoi(strSize);
    } else if ((pos = strLine.find(MessgeTimeKeyword)) != std::string::npos) {
      auto posMessageNameEnd =
          strLine.find(' ', pos + MessgeTimeKeyword.length());
      std::string strMessageName;
      if (posMessageNameEnd != std::string::npos) {
        strMessageName = strLine.substr(
            pos + MessgeTimeKeyword.length(),
            posMessageNameEnd - pos - MessgeTimeKeyword.length());
      }

      auto posMessageTimeEnd = strLine.find(' ', posMessageNameEnd + 1);
      std::string strTime;
      if (posMessageTimeEnd != std::string::npos) {
        strTime = strLine.substr(posMessageNameEnd + 1,
                                 posMessageTimeEnd - posMessageNameEnd - 1);
      }

      MessageSizeTime messageSizeTime{};
      messageSizeTime.size = mapMessageSize[strMessageName];
      messageSizeTime.time = std::stoi(strTime);

      resultMessageSizeTime[strMessageName].push_back(messageSizeTime);
    }
  }

  fs.close();
  return true;
}

void printResult(const std::string& strFileName) {
  std::ofstream fs(strFileName, std::ofstream::out);
  if (!fs.is_open()) {
    std::cout << "Failed to open file " + strFileName;
    return;
  }

  fs << "Message Name"
     << "\t"
     << "Size"
     << "\t"
     << "Process Time(us)" << std::endl;
  for (const auto& messageSizeTime : resultMessageSizeTime) {
    auto messageName = messageSizeTime.first;
    for (const auto& sizeTime : messageSizeTime.second) {
      fs << messageName << "\t" << sizeTime.size << "\t" << sizeTime.time
         << std::endl;
    }
    auto minMaxSizeIter = std::minmax_element(
        messageSizeTime.second.begin(), messageSizeTime.second.end(),
        [](const MessageSizeTime& element1, const MessageSizeTime& element2) {
          return element1.size < element2.size;
        });

    fs << "Min Size"
       << "\t" << minMaxSizeIter.first->size << std::endl;
    fs << "Max Size"
       << "\t" << minMaxSizeIter.second->size << std::endl;

    auto minMaxTimeIter = std::minmax_element(
        messageSizeTime.second.begin(), messageSizeTime.second.end(),
        [](const MessageSizeTime& element1, const MessageSizeTime& element2) {
          return element1.time < element2.time;
        });

    fs << "Min Time"
       << "\t" << minMaxTimeIter.first->time << std::endl;
    fs << "Max Time"
       << "\t" << minMaxTimeIter.second->time << std::endl;

    fs << std::endl;
  }
  fs.close();
}

using namespace std;

int main(int argc, const char* argv[]) {
  try {
    std::string strFileName;
    std::string strResultName;

    po::options_description desc("Options");

    desc.add_options()("help,h", "Print help messages")(
        "l,log-file-name", po::value<string>(&strFileName)->required(),
        "zilliqa log file name")("r,result-file-name",
                                 po::value<string>(&strResultName)->required(),
                                 "grep result file name");

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);

      /** --help option
       */
      if (vm.count("help")) {
        SWInfo::LogBrandBugReport();
        cout << desc << endl;
        return SUCCESS;
      }
      po::notify(vm);
    } catch (boost::program_options::required_option& e) {
      SWInfo::LogBrandBugReport();
      cerr << "ERROR: " << e.what() << endl << endl;
      cout << desc;
      return ERROR_IN_COMMAND_LINE;
    } catch (boost::program_options::error& e) {
      SWInfo::LogBrandBugReport();
      cerr << "ERROR: " << e.what() << endl << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    if (grepFile(strFileName)) {
      printResult(strResultName);
      std::cout << "Grep performance result successfully write into "
                << strResultName << std::endl;
    }

  } catch (exception& e) {
    cerr << "Unhandled Exception reached the top of main: " << e.what()
         << ", application will now exit" << endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }
  return SUCCESS;
}
