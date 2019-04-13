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
  std::unique_ptr<Json::StreamWriter> m_writer;
  std::unique_ptr<Json::CharReader> m_reader;

  std::mutex m_mutexWriter;
  std::mutex m_mutexReader;

  JSONUtils() {
    Json::CharReaderBuilder readBuilder;
    m_reader = std::unique_ptr<Json::CharReader>(readBuilder.newCharReader());

    Json::StreamWriterBuilder writeBuilder;
    writeBuilder["commentStyle"] = "None";
    writeBuilder["indentaion"] = "";
    m_writer =
        std::unique_ptr<Json::StreamWriter>(writeBuilder.newStreamWriter());
  }

  ~JSONUtils(){};

 public:
  static JSONUtils& GetInstance() {
    static JSONUtils jsonutils;
    return jsonutils;
  }

  /// Convert a string to Json object
  bool convertStrtoJson(const std::string& str, Json::Value& dstObj) {
    bool result = true;
    try {
      std::string errors;
      std::lock_guard<std::mutex> g(m_mutexReader);
      if (!m_reader->parse(str.c_str(), str.c_str() + str.size(), &dstObj,
                           &errors)) {
        LOG_GENERAL(WARNING, "Corrupted JSON: " << errors);
        result = false;
      }
    } catch (const std::exception& e) {
      LOG_GENERAL(WARNING, "Exception caught: " << e.what());
      result = false;
    }
    return result && (dstObj.isObject() || dstObj.isArray());
  }

  /// Convert a Json object to string
  std::string convertJsontoStr(const Json::Value& _json) {
    std::ostringstream oss;
    std::lock_guard<std::mutex> g(m_mutexWriter);
    m_writer->write(_json, &oss);
    return oss.str();
  }

  /// Write a Json object to target file
  void writeJsontoFile(const std::string& path, const Json::Value& _json) {
    std::ofstream os(path);
    std::lock_guard<std::mutex> g(m_mutexWriter);
    m_writer->write(_json, &os);
  }
};

#endif  // __JSONUTILS_H__
