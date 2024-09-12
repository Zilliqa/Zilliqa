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

#ifndef ZILLIQA_SRC_LIBUTILS_JSONUTILS_H_
#define ZILLIQA_SRC_LIBUTILS_JSONUTILS_H_

#include <json/json.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

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

  ~JSONUtils() {};

 public:
  static JSONUtils& GetInstance() {
    static JSONUtils jsonutils;
    return jsonutils;
  }

  bool getUint128FromObject(const Json::Value& obj, const std::string& key,
                            uint128_t& result) const {
    return this->getUint128FromObject(obj, key.c_str(), result);
  }

  /// Get an object value as a uint128_t.
  /// @return false if we failed, true if we succeeded.
  bool getUint128FromObject(const Json::Value& obj, const char* key,
                            uint128_t& result) const {
    if (obj.isObject() && obj.isMember(key)) {
      Json::Value member = obj[key];
      if (member.isString()) {
        // Parse it.
        try {
          result =
              DataConversion::ConvertStrToInt<uint128_t>(member.asString());
          return true;
        } catch (...) {
          return false;
        }
      } else if (member.isIntegral()) {
        result = member.asUInt64();
      }
      return true;
    }
    return false;
  }

  /// Convert a string to Json object
  bool convertStrtoJson(const std::string& str, Json::Value& dstObj) {
    bool result = true;
    try {
      std::string errors;
      std::lock_guard<std::mutex> g(m_mutexReader);
      if (!m_reader->parse(str.c_str(), str.c_str() + str.size(), &dstObj,
                           &errors)) {
        errors.erase(std::remove_if(errors.begin(), errors.end(),
                                    [&](char ch) {
                                      return std::iscntrl(
                                          static_cast<unsigned char>(ch));
                                    }),
                     errors.end());
        LOG_GENERAL(WARNING, "Corrupted string: " << str);
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

  static std::size_t hashJsonValue(const Json::Value& log) {
    Json::StreamWriterBuilder writer;
    std::string logStr = Json::writeString(writer, log);
    return std::hash<std::string>{}(logStr);
  }

  static bool equalJsonValue(const Json::Value& lhs, const Json::Value& rhs) {
    return lhs == rhs;
  }

  Json::Value FilterDuplicateLogs(const Json::Value& logs) {
    using CustomHash = std::function<std::size_t(const Json::Value&)>;
    using CustomEqual =
        std::function<bool(const Json::Value&, const Json::Value&)>;

    std::unordered_set<Json::Value, CustomHash, CustomEqual> uniqueLogs(
        10, hashJsonValue, equalJsonValue);
    Json::Value filteredLogs(Json::arrayValue);

    for (const auto& log : logs) {
      if (uniqueLogs.insert(log).second) {
        filteredLogs.append(log);
      }
    }
    return filteredLogs;
  }
};

#endif  // ZILLIQA_SRC_LIBUTILS_JSONUTILS_H_
