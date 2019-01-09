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

#ifndef __JSONUTILS_H__
#define __JSONUTILS_H__

#include <json/json.h>
#include <memory>
#include <sstream>
#include <string>

class JSONUtils {
 public:
  // Convert a string to Json object
  static bool convertStrtoJson(const std::string& str, Json::Value& dstObj) {
    Json::CharReaderBuilder readBuilder;
    std::unique_ptr<Json::CharReader> reader(readBuilder.newCharReader());
    std::string errors;
    if (!reader->parse(str.c_str(), str.c_str() + str.size(), &dstObj,
                       &errors)) {
      LOG_GENERAL(WARNING,
                  "The Json is corrupted, failed to parse: " << errors);
      return false;
    }
    return true;
  }
  // Convert a Json object to string
  static std::string convertJsontoStr(const Json::Value& _json) {
    Json::StreamWriterBuilder writeBuilder;
    std::unique_ptr<Json::StreamWriter> writer(writeBuilder.newStreamWriter());
    std::ostringstream oss;
    writer->write(_json, &oss);
    return oss.str();
  }
  // Write a Json object to target file
  static void writeJsontoFile(const std::string& path,
                              const Json::Value& _json) {
    Json::StreamWriterBuilder writeBuilder;
    std::unique_ptr<Json::StreamWriter> writer(writeBuilder.newStreamWriter());
    std::ofstream os(path);
    writer->write(_json, &os);
  }
};

#endif  // __JSONUTILS_H__
