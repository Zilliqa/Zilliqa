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

#include <cassert>
#include <sstream>

#include <boost/algorithm/string.hpp>

#include "FiltersUtils.h"

#include "libUtils/Logger.h"

namespace evmproj {
namespace filters {

const char *FROMBLOCK_STR = "fromBlock";
const char *TOBLOCK_STR("toBlock");
const char *LATEST_STR("latest");
const char *EARLIEST_STR("earliest");
const char *PENDING_STR("pending");
const char *ADDRESS_STR("address");
const char *TOPICS_STR("topics");
const char *LOGINDEX_STR("logIndex");
const char *BLOCKNUMBER_STR("blockNumber");
const char *BLOCKHASH_STR("blockHash");
const char *TRANSACTIONHASH_STR("transactionHash");
const char *TRANSACTIONINDEX_STR("transactionIndex");
const char *DATA_STR("data");

namespace {
constexpr char EVENT_FILTER_SUFFIX = 'a';
constexpr char TXN_FILTER_SUFFIX = 'b';
constexpr char BLK_FILTER_SUFFIX = 'c';

char Suffix(FilterType type) {
  assert(type != FilterType::INVALID);

  switch (type) {
    case FilterType::EVENT_FILTER:
      return EVENT_FILTER_SUFFIX;
    case FilterType::TXN_FILTER:
      return TXN_FILTER_SUFFIX;
    case FilterType::BLK_FILTER:
      return BLK_FILTER_SUFFIX;
    default:
      break;
  }

  throw std::runtime_error(std::string("Invalid filter type " +
                                       std::to_string(static_cast<int>(type))));
}

Json::CharReader &GetJsonReader() {
  static thread_local Json::CharReaderBuilder builder;
  static thread_local std::unique_ptr<Json::CharReader> reader(
      builder.newCharReader());
  return *reader;
}

Json::StreamWriter &GetJsonWriter() {
  static thread_local Json::StreamWriterBuilder builder;
  builder["indentation"] = Json::Value{};
  static thread_local std::unique_ptr<Json::StreamWriter> writer(
      builder.newStreamWriter());
  return *writer;
}

}  // namespace

Json::Value JsonRead(const std::string &str, std::string &error) {
  Json::Value ret;
  if (str.empty()) {
    error = "input string is empty";
  } else {
    error.clear();
    GetJsonReader().parse(str.data(), str.data() + str.size(), &ret, &error);
  }
  return ret;
}

std::string JsonWrite(const Json::Value &json) {
  std::ostringstream os;
  GetJsonWriter().write(json, &os);
  return os.str();
}

std::string NumberAsString(uint64_t number) {
  std::stringstream result;
  result << "0x" << std::hex << number;
  return result.str();
}

std::string NormalizeHexString(const std::string &str) {
  if (str.size() < 2 || str[1] != 'x') {
    return std::string("0x") + str;
  }
  return str;
}

std::string NormalizeEventData(const Json::Value &data) {
  std::stringstream result;
  if (data.isArray()) {
    result << "0x";
    for (const auto &v : data) {
      if (!v.isUInt()) {
        LOG_GENERAL(WARNING, "Expected array of uints in " << JsonWrite(data));
        break;
      }
      result << std::hex << v.asUInt();
    }
  } else if (data.isString()) {
    result << data.asString();
  }
  return result.str();
}

FilterId NewFilterId(uint64_t counter, FilterType type) {
  std::stringstream result;
  result << "0x" << std::hex << counter << Suffix(type);
  return result.str();
}

FilterType GuessFilterType(const FilterId &id) {
  if (id.size() >= 3) {
    switch (id.back()) {
      case EVENT_FILTER_SUFFIX:
        return FilterType::EVENT_FILTER;
      case TXN_FILTER_SUFFIX:
        return FilterType::TXN_FILTER;
      case BLK_FILTER_SUFFIX:
        return FilterType::BLK_FILTER;
      default:
        break;
    }
  }
  return FilterType::INVALID;
}

uint64_t ExtractNumber(std::string str, std::string &error) {
  error.clear();

  if (str.size() >= 2 && str[1] == 'x') {
    if (str[0] != '0') {
      error = "Param parse error, 0x expected";
      return 0;
    }
    str = str.substr(2);
  }

  if (str.empty()) {
    error = "Hex string is empty";
    return 0;
  }

  try {
    uint64_t number = std::stoull(str, nullptr, 16);
    error.clear();
    return number;
  } catch (const std::exception &e) {
    error = "Param parse error: ";
    error += e.what();
  }

  return 0;
}

EpochNumber ExtractEpochFromParam(std::string str, std::string &error) {
  error.clear();

  if (str.empty()) {
    error = "Block number param parse error, empty string";
    return SEEN_NOTHING;
  }

  if (str == EARLIEST_STR) {
    return EARLIEST_EPOCH;
  }

  if (std::isxdigit(str[0])) {
    auto number = ExtractNumber(std::move(str), error);
    if (!error.empty()) {
      return SEEN_NOTHING;
    }
    return number;
  }

  if (str == LATEST_STR) {
    return LATEST_EPOCH;
  }

  if (str == PENDING_STR) {
    return PENDING_EPOCH;
  }

  error = "Block number param parse error: ";
  error += str;
  return SEEN_NOTHING;
}

uint64_t ExtractNumberFromJsonObj(const Json::Value &obj, const char *key,
                                  std::string &error, bool &found) {
  found = false;

  auto str = ExtractStringFromJsonObj(obj, key, error, found);
  if (!found) {
    return 0;
  }

  auto number = ExtractNumber(std::move(str), error);
  if (error.empty()) {
    found = true;
  }

  return number;
}

std::string ExtractStringFromJsonObj(const Json::Value &obj, const char *key,
                                     std::string &error, bool &found) {
  found = false;

  auto value = obj.get(key, Json::Value{});
  if (value.isNull()) {
    return {};
  }

  if (!value.isString()) {
    error = "String value expected";
    return {};
  }

  found = true;
  auto foundValue = value.asString();
  boost::algorithm::to_lower(foundValue);
  return foundValue;
}

Json::Value ExtractArrayFromJsonObj(const Json::Value &obj, const char *key,
                                    std::string &error) {
  auto empty_array = Json::Value(Json::arrayValue);
  auto v = obj.get(key, empty_array);
  if (!v.isArray()) {
    error = "Json array expected";
    v = empty_array;
  }
  return v;
}

bool ExtractTopicFilter(const Json::Value &topic, EventFilterParams &filter,
                        std::string &error) {
  if (topic.isNull()) {
    filter.topicMatches.emplace_back();
    return true;
  }

  if (topic.isString()) {
    if (topic.empty()) {
      error = "Invalid topic filter: empty string";
      return false;
    }
    filter.topicMatches.emplace_back();
    auto topicStr = topic.asString();
    boost::algorithm::to_lower(topicStr);
    filter.topicMatches.back().emplace_back(topicStr);
    return true;
  }

  if (!topic.isArray()) {
    error = "Invalid topic filter: array expected";
    return false;
  }

  filter.topicMatches.emplace_back();

  if (topic.empty()) {
    return true;
  }

  auto &variants = filter.topicMatches.back();
  for (const auto &value : topic) {
    if (!value.isString() || value.empty()) {
      error = "Invalid topic filter: parse error";
      return false;
    }
    auto valueStr = value.asString();
    boost::algorithm::to_lower(valueStr);
    variants.emplace_back(valueStr);
  }

  return true;
}

bool ExtractTopicFilters(const Json::Value &topics, EventFilterParams &filter,
                         std::string &error) {
  if (!topics.isArray()) {
    error = "Invalid event filter params (not an array)";
    return false;
  }

  if (topics.empty()) {
    return true;
  }

  if (topics.size() > 4) {
    error = "Size of filter topics exceed 4";
    return false;
  }

  for (const auto &topic : topics) {
    if (!ExtractTopicFilter(topic, filter, error)) {
      return false;
    }
  }

  while (!filter.topicMatches.empty()) {
    if (filter.topicMatches.back().empty()) {
      filter.topicMatches.pop_back();
    } else {
      break;
    }
  }

  return true;
}

bool InitializeEventFilter(const Json::Value &params, EventFilterParams &filter,
                           std::string &error) {
  if (!params.isObject()) {
    error = "Invalid event filter params (not an object)";
    return false;
  }

  bool found = false;
  error.clear();

  auto str = ExtractStringFromJsonObj(params, FROMBLOCK_STR, error, found);
  if (found) {
    filter.fromBlock = ExtractEpochFromParam(std::move(str), error);
  }
  if (!error.empty()) {
    return false;
  }

  str = ExtractStringFromJsonObj(params, TOBLOCK_STR, error, found);
  if (found) {
    filter.toBlock = ExtractEpochFromParam(std::move(str), error);
  }
  if (!error.empty()) {
    return false;
  }

  std::vector<std::string> addresses;
  auto empty_array = Json::Value(Json::arrayValue);
  auto v = params.get(ADDRESS_STR, empty_array);
  if (v.isArray()) {
    for (const auto &address : v) {
      if (!address.isString()) {
        error = "Addresses must be strings";
        return false;
      }
      addresses.push_back(address.asString());
    }
  } else if (v.isString()) {
    addresses.push_back(v.asString());
  } else {
    error = "Address must be an array or a string";
    return false;
  }
  if (addresses.size() > 16) {
    error = "Address cannot contain more than 16 elements";
    return false;
  }
  filter.address = addresses;

  auto topics = ExtractArrayFromJsonObj(params, TOPICS_STR, error);
  if (!error.empty()) {
    return false;
  }

  return ExtractTopicFilters(topics, filter, error);
}

bool Match(const EventFilterParams &filter, const Address &address,
           const std::vector<Quantity> &topics) {
  if (!filter.address.empty()) {
    // We linearly search the address filter here. Since we limit the length of
    // the filter to 16 addresses, this is acceptable.
    auto v = filter.address;
    if (std::find_if(v.begin(), v.end(), [&address](const auto &a) {
          return boost::iequals(address, a);
        }) == v.end()) {
      return false;
    }
  }

  if (filter.topicMatches.empty()) {
    return true;
  }

  size_t i = 0;
  size_t total = topics.size();
  for (const auto &topicMatch : filter.topicMatches) {
    if (total <= i) {
      break;
    }

    const auto topic = boost::to_lower_copy(topics[i++]);

    if (topicMatch.empty()) {
      continue;
    }

    bool found = false;
    for (const auto &t : topicMatch) {
      if (boost::iequals(t, topic)) {
        found = true;
        break;
      }
    }

    if (!found) {
      return false;
    }
  }

  return true;
}

namespace {

Json::Value CreateEventResponseTemplate() {
  Json::Value v;
  Json::Value zero("0x0");
  v[LOGINDEX_STR] = zero;
  v[BLOCKHASH_STR] = zero;
  v[TRANSACTIONINDEX_STR] = zero;
  return v;
}

}  // namespace

Json::Value CreateEventResponseItem(EpochNumber epoch, const TxnHash &tx_hash,
                                    const Address &address,
                                    const std::vector<Quantity> &topics,
                                    const Json::Value &data) {
  static const Json::Value item = CreateEventResponseTemplate();

  Json::Value v(item);

  v[BLOCKNUMBER_STR] = NumberAsString(epoch);
  v[TRANSACTIONHASH_STR] = tx_hash;
  v[ADDRESS_STR] = address;
  v[DATA_STR] = NormalizeEventData(data);

  auto json_topics = Json::Value(Json::arrayValue);
  for (const auto &t : topics) {
    json_topics.append(t);
  }
  v[TOPICS_STR] = json_topics;

  return v;
}

}  // namespace filters
}  // namespace evmproj
