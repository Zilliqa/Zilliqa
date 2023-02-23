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

#include <array>

#include "libEth/filters/FiltersUtils.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE filtersapitest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace evmproj::filters;
using namespace std::string_literals;

struct Fixture {
  Fixture() { INIT_STDOUT_LOGGER() }
};

BOOST_GLOBAL_FIXTURE(Fixture);

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(conversions) {
  BOOST_REQUIRE(NumberAsString(0) == "0x0"s);

  BOOST_REQUIRE(NumberAsString(0xffffffffffffffffull) == "0xffffffffffffffff"s);

  BOOST_REQUIRE(NormalizeHexString("234abcde") == "0x234abcde"s);
  BOOST_REQUIRE(NormalizeHexString("0x234abcde") == "0x234abcde"s);
  BOOST_REQUIRE(NormalizeHexString("0x") == "0x"s);
  BOOST_REQUIRE(NormalizeHexString("0") == "0x0"s);
  BOOST_REQUIRE(NormalizeHexString("") == "0x"s);

  std::string error;

  Json::Value event_data = JsonRead(
      "[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
      "0,0,0,0,0,0,0,0,0,0,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
      "0,0,0,0,0,0,0,0,12,72,101,108,108,111,32,87,111,114,108,100,33,0,0,0,0,"
      "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]"s,
      error);
  BOOST_REQUIRE(error.empty());

  BOOST_REQUIRE(
      NormalizeEventData(event_data) ==
      "0x0000000000000000000000000000000200000000000000000000000000000000c48656c6c6f20576f726c642100000000000000000000"s);

  BOOST_REQUIRE(ExtractNumber("0xffffffffffffffff"s, error) ==
                0xffffffffffffffffull);
  BOOST_REQUIRE(error.empty());

  BOOST_REQUIRE(ExtractNumber("0xabcDEf012"s, error) == 0xabcdef012ull);
  BOOST_REQUIRE(error.empty());

  BOOST_REQUIRE(ExtractNumber("QQQQabcDEf012"s, error) == 0);
  BOOST_REQUIRE(!error.empty());
  error.clear();

  BOOST_REQUIRE(ExtractNumber(""s, error) == 0);
  BOOST_REQUIRE(!error.empty());
  error.clear();

  BOOST_REQUIRE(ExtractEpochFromParam("0x2020"s, error) == 0x2020);
  BOOST_REQUIRE(error.empty());

  BOOST_REQUIRE(ExtractEpochFromParam("latest"s, error) == LATEST_EPOCH);
  BOOST_REQUIRE(error.empty());

  BOOST_REQUIRE(ExtractEpochFromParam("earliest"s, error) == EARLIEST_EPOCH);
  BOOST_REQUIRE(error.empty());

  BOOST_REQUIRE(ExtractEpochFromParam("pending"s, error) == PENDING_EPOCH);
  BOOST_REQUIRE(error.empty());

  BOOST_REQUIRE(ExtractEpochFromParam("worst"s, error) == SEEN_NOTHING);
  BOOST_REQUIRE(!error.empty());
  error.clear();

  BOOST_REQUIRE(ExtractEpochFromParam(""s, error) == SEEN_NOTHING);
  BOOST_REQUIRE(!error.empty());
  error.clear();

  BOOST_REQUIRE(ExtractEpochFromParam("hohoho"s, error) == SEEN_NOTHING);
  BOOST_REQUIRE(!error.empty());
  error.clear();

  BOOST_REQUIRE(ExtractEpochFromParam("0x"s, error) == SEEN_NOTHING);
  BOOST_REQUIRE(!error.empty());
  error.clear();

  const auto STR = "xxx43210"s;
  const auto NUM = "0x123456"s;

  Json::Value obj;
  obj["n"] = NUM;
  obj["s"] = STR;
  Json::Value array(Json::arrayValue);
  array.append(obj);
  obj["a"] = array;

  bool found = false;

  BOOST_REQUIRE(ExtractStringFromJsonObj(obj, "s", error, found) == STR);
  BOOST_REQUIRE(found);
  BOOST_REQUIRE(error.empty());
  found = false;

  BOOST_REQUIRE(ExtractStringFromJsonObj(obj, "ssss", error, found) == ""s);

  // if the field is just not found, there's no error
  BOOST_REQUIRE(!found);
  BOOST_REQUIRE(error.empty());

  BOOST_REQUIRE(ExtractStringFromJsonObj(obj, "a", error, found) == ""s);
  BOOST_REQUIRE(!found);

  // if the field is of wrong type, error is set
  BOOST_REQUIRE(!error.empty());  // not a string
  error.clear();

  BOOST_REQUIRE(ExtractNumberFromJsonObj(obj, "n", error, found) == 0x123456);
  BOOST_REQUIRE(found);
  BOOST_REQUIRE(error.empty());
  found = false;

  BOOST_REQUIRE(ExtractNumberFromJsonObj(obj, "ssss", error, found) == 0);

  // if the field is just not found, there's no error
  BOOST_REQUIRE(!found);
  BOOST_REQUIRE(error.empty());

  BOOST_REQUIRE(ExtractNumberFromJsonObj(obj, "a", error, found) == 0);
  BOOST_REQUIRE(!found);

  // if the field is of wrong type, error is set
  BOOST_REQUIRE(!error.empty());  // not a number
  error.clear();
}

BOOST_AUTO_TEST_CASE(filter_ids) {
  BOOST_REQUIRE(GuessFilterType(NewFilterId(1234, FilterType::EVENT_FILTER)) ==
                FilterType::EVENT_FILTER);
  BOOST_REQUIRE(GuessFilterType(NewFilterId(1234, FilterType::TXN_FILTER)) ==
                FilterType::TXN_FILTER);
  BOOST_REQUIRE(GuessFilterType(NewFilterId(1234, FilterType::BLK_FILTER)) ==
                FilterType::BLK_FILTER);
  BOOST_REQUIRE(GuessFilterType("") == FilterType::INVALID);
  BOOST_REQUIRE(GuessFilterType("0x") == FilterType::INVALID);
  BOOST_REQUIRE(GuessFilterType("0x232") == FilterType::INVALID);
  BOOST_REQUIRE(GuessFilterType("0x0101010") == FilterType::INVALID);
}

namespace {

const auto SOME_ADDRESS = "0xdeadbeef012345678"s;
const auto OTHER_ADDRESS = "0xdeadbeef01234567f"s;

std::string ToString(const std::vector<std::string>& v) {
  std::ostringstream os;
  os << "[";
  for (const auto& s : v) {
    os << s << ",";
  }
  os << "]";
  return os.str();
}

const std::vector<std::string> VALID_TOPIC_FILTERS = {
    "[null]"s,                                            // 0
    "[]"s,                                                // 1
    R"([ "0x2222" ])"s,                                   // 2
    R"([ "0x1111", "0x2222" ])"s,                         // 3
    R"([ "0x1111", "0x2222", "0x3333" ])"s,               // 4
    R"([ "0x1111", "0x2222", "0x3333", "0x4444" ])"s,     // 5
    R"([ "0x1111", null, "0x3333", "0x4444" ])"s,         // 6
    R"([ "0x1111", [], "0x3333", "0x4444" ])"s,           // 7
    R"([ ["0x1111", "0x2222"], "0x3333", "0x4444" ])"s,   // 8
    R"([ ["0x1111", "0x2222", "0x3333"], "0x4444" ])"s};  // 9

const std::vector<std::vector<std::string>> SAMPLE_TOPICS = {
    {"0x1111"s, "0x2222"s, "0x3333"s, "0x4444"s},  // 0
    {"0x1111"s},                                   // 1
    {"0x4444"s},                                   // 2
    {"0x1111"s, "0x4444"s},                        // 3
    {"0x2222"s, "0x4444"s},                        // 4
    {"0x4444"s, "0x4444"s},                        // 5
    {"0x2222"s, "0x3333"s, "0x4444"s},             // 6
    {"0x3333"s, "0x3333"s, "0x4444"s},             // 7
    {"0x2222"s, "0x5555"s, "0x4444"s},             // 8
};

const std::array<std::array<int, 10>, 9> EXPECTED_MATCHES = {
    // 0  1  2  3  4  5  6  7  8  9
    {{1, 1, 0, 1, 1, 1, 1, 1, 0, 0},    // 0
     {1, 1, 0, 1, 1, 1, 1, 1, 1, 1},    // 1
     {1, 1, 0, 0, 0, 0, 0, 0, 0, 0},    // 2
     {1, 1, 0, 0, 0, 0, 1, 1, 0, 1},    // 3
     {1, 1, 1, 0, 0, 0, 0, 0, 0, 1},    // 4
     {1, 1, 0, 0, 0, 0, 0, 0, 0, 0},    // 5
     {1, 1, 1, 0, 0, 0, 0, 0, 1, 0},    // 6
     {1, 1, 0, 0, 0, 0, 0, 0, 0, 0},    // 7
     {1, 1, 1, 0, 0, 0, 0, 0, 0, 0}}};  // 8

const std::vector<std::string> INVALID_TOPIC_FILTERS = {
    "{}"s,
    "222"s,
    R"([ 4 ])"s,
    R"([ "0x1111", 3000 ])"s,
    R"([ "0x1111", "0x2222", "0x3333", "0x4444", "0x5555" ])"s,
    R"([ {"0x1111":"0x2222"}, "0x3333", "0x4444" ])"s,
    R"([ [[]]], "0x4444" ])"s};

Json::Value MakeValidEventFilter() {
  Json::Value v;
  v[FROMBLOCK_STR] = "0x2222";
  v[TOBLOCK_STR] = "latest";
  v[ADDRESS_STR] = SOME_ADDRESS;
  Json::Value& topics = v[TOPICS_STR];
  topics.append("0x1111");
  topics.append("0x2222");
  topics.append("0x3333");
  topics.append("0x4444");
  return v;
}

}  // namespace

BOOST_AUTO_TEST_CASE(initialize_event_filter) {
  Json::Value valid_params = MakeValidEventFilter();

  std::string error;
  EventFilterParams filter;
  BOOST_REQUIRE(InitializeEventFilter(valid_params, filter, error));
  BOOST_REQUIRE(error.empty());

  auto invalid_params = valid_params;
  invalid_params[ADDRESS_STR] = 2.222;
  BOOST_REQUIRE(!InitializeEventFilter(invalid_params, filter, error));
  BOOST_REQUIRE(!error.empty());
  error.clear();

  invalid_params = valid_params;
  invalid_params[FROMBLOCK_STR] = 2.222;
  BOOST_REQUIRE(!InitializeEventFilter(invalid_params, filter, error));
  BOOST_REQUIRE(!error.empty());
  error.clear();

  invalid_params = valid_params;
  invalid_params[TOBLOCK_STR] = 2.222;
  BOOST_REQUIRE(!InitializeEventFilter(invalid_params, filter, error));
  BOOST_REQUIRE(!error.empty());
  error.clear();

  invalid_params = valid_params;
  invalid_params[TOBLOCK_STR] = 2.222;
  BOOST_REQUIRE(!InitializeEventFilter(invalid_params, filter, error));
  BOOST_REQUIRE(!error.empty());
  error.clear();

  invalid_params = valid_params;
  invalid_params[TOPICS_STR] = 2.222;
  BOOST_REQUIRE(!InitializeEventFilter(invalid_params, filter, error));
  BOOST_REQUIRE(!error.empty());
  error.clear();

  invalid_params = valid_params;
  invalid_params[TOPICS_STR].append(302010);
  BOOST_REQUIRE(!InitializeEventFilter(invalid_params, filter, error));
  BOOST_REQUIRE(!error.empty());
  error.clear();

  invalid_params = valid_params;
  invalid_params[TOPICS_STR][2] = true;
  BOOST_REQUIRE(!InitializeEventFilter(invalid_params, filter, error));
  BOOST_REQUIRE(!error.empty());
  error.clear();
}

BOOST_AUTO_TEST_CASE(event_filter_initialize) {
  Json::Value valid_params = MakeValidEventFilter();
  std::string error;
  EventFilterParams filter;
  BOOST_REQUIRE(InitializeEventFilter(valid_params, filter, error));

  Json::Value topics;
  Json::Value params;

  for (const auto& str : VALID_TOPIC_FILTERS) {
    topics = JsonRead(str, error);
    BOOST_REQUIRE(error.empty());
    params[TOPICS_STR] = topics;
    EventFilterParams f;
    BOOST_REQUIRE(InitializeEventFilter(params, f, error));
  }

  for (const auto& str : INVALID_TOPIC_FILTERS) {
    topics = JsonRead(str, error);
    BOOST_REQUIRE(error.empty());
    params[TOPICS_STR] = topics;
    EventFilterParams f;
    BOOST_REQUIRE(!InitializeEventFilter(params, f, error));
    BOOST_REQUIRE(!error.empty());
    error.clear();
  }
}

BOOST_AUTO_TEST_CASE(event_filter_match) {
  std::vector<EventFilterParams> filters;
  for (const auto& str : VALID_TOPIC_FILTERS) {
    filters.emplace_back();
    auto& f = filters.back();
    Json::Value json;
    std::string error;
    BOOST_REQUIRE_MESSAGE(error.empty(), "Error: " << error);
    json[TOPICS_STR] = JsonRead(str, error);
    BOOST_REQUIRE(InitializeEventFilter(json, f, error));
  }

  // 1. Check topic matches according to the table

  for (size_t topic_n = 0; topic_n < EXPECTED_MATCHES.size(); ++topic_n) {
    const auto& matches = EXPECTED_MATCHES[topic_n];
    for (size_t filter_n = 0; filter_n < matches.size(); ++filter_n) {
      bool expected = matches[filter_n] != 0;
      bool got = Match(filters[filter_n], SOME_ADDRESS, SAMPLE_TOPICS[topic_n]);
      BOOST_REQUIRE_MESSAGE(
          got == expected,
          topic_n << " " << filter_n << "\n"
                  << "Expected Match(" << VALID_TOPIC_FILTERS[filter_n] << ", "
                  << ToString(SAMPLE_TOPICS[topic_n]) << expected);
    }
  }

  // 2. Match should return false if event's address don't match

  const auto& topics = SAMPLE_TOPICS[3];
  for (auto& f : filters) {
    f.address = {SOME_ADDRESS};
    BOOST_REQUIRE(Match(f, OTHER_ADDRESS, topics) == false);
  }
}

BOOST_AUTO_TEST_CASE(install_filters_result) {
  auto meta = APICache::Create();

  auto& api = meta->GetFilterAPI();
  auto& update = meta->GetUpdate();

  auto filter_params = MakeValidEventFilter();

  // 1. the cache has no epoch yet, installing filters fails

  auto install_res = api.InstallNewBlockFilter();
  BOOST_REQUIRE(install_res.success == false);

  install_res = api.InstallNewPendingTxnFilter();
  BOOST_REQUIRE(install_res.success == false);

  install_res = api.InstallNewEventFilter(filter_params);
  BOOST_REQUIRE(install_res.success == false);

  update.StartEpoch(1, NumberAsString(1) + "_hash", 3, 0);

  // 2. If the cache is initialized, filters can be installed

  install_res = api.InstallNewBlockFilter();
  BOOST_REQUIRE(install_res.success == true);
  BOOST_REQUIRE(GuessFilterType(install_res.result) == FilterType::BLK_FILTER);

  install_res = api.InstallNewPendingTxnFilter();
  BOOST_REQUIRE(install_res.success == true);
  BOOST_REQUIRE(GuessFilterType(install_res.result) == FilterType::TXN_FILTER);

  install_res = api.InstallNewEventFilter(filter_params);
  BOOST_REQUIRE(install_res.success == true);
  BOOST_REQUIRE(GuessFilterType(install_res.result) ==
                FilterType::EVENT_FILTER);

  // 3. Invalid event filter params

  std::vector<Json::Value> inv;
  inv.reserve(8);

  inv.emplace_back(filter_params);
  inv.back()[FROMBLOCK_STR] = "xxx";

  inv.emplace_back(filter_params);
  inv.back()[TOBLOCK_STR] = "fastest";

  inv.emplace_back(filter_params);
  inv.back()[TOBLOCK_STR] = 202020;

  inv.emplace_back(filter_params);
  inv.back()[ADDRESS_STR] = 202020;

  inv.emplace_back(filter_params);
  inv.back()[ADDRESS_STR] = Json::Value(Json::arrayValue);
}

BOOST_AUTO_TEST_SUITE_END()
