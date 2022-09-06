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

#ifndef ZILLIQA_SRC_LIBETH_FILTERS_FILTERUTILS_H_
#define ZILLIQA_SRC_LIBETH_FILTERS_FILTERUTILS_H_

#include "Common.h"

namespace evmproj {
namespace filters {

extern const char *FROMBLOCK_STR;
extern const char *TOBLOCK_STR;
extern const char *LATEST_STR;
extern const char *EARLIEST_STR;
extern const char *PENDING_STR;
extern const char *ADDRESS_STR;
extern const char *TOPICS_STR;
extern const char *LOGINDEX_STR;
extern const char *BLOCKNUMBER_STR;
extern const char *BLOCKHASH_STR;
extern const char *TRANSACTIONHASH_STR;
extern const char *TRANSACTIONINDEX_STR;
extern const char *DATA_STR;

enum class FilterType {
  INVALID,
  EVENT_FILTER,
  TXN_FILTER,
  BLK_FILTER,
};

// TODO propose it to libUtils
Json::Value JsonRead(const std::string &str, std::string &error);

// TODO propose it to libUtils
std::string JsonWrite(const Json::Value &json);

/// Number -> "0x..."
std::string NumberAsString(uint64_t number);

/// Creates a new filter id by type and incremental counter
FilterId NewFilterId(uint64_t counter, FilterType type);

/// Determines filter type
FilterType GuessFilterType(const FilterId &id);

uint64_t ExtractNumber(std::string str, std::string &error);

/// Tries to extract epoch number from string RPC parameter, with processing
/// of special values ("latest", "pending", "earliest");
EpochNumber ExtractEpochFromParam(std::string str, std::string &error);

uint64_t ExtractNumberFromJsonObj(const Json::Value &obj, const char *key,
                                  std::string &error, bool &found);

std::string ExtractStringFromJsonObj(const Json::Value &obj, const char *key,
                                     std::string &error, bool &found);

Json::Value ExtractArrayFromJsonObj(const Json::Value &obj, const char *key,
                                    std::string &error);

bool ExtractTopicFilter(const Json::Value &topic, EventFilterParams &filter,
                        std::string &error);

bool ExtractTopicFilters(const Json::Value &topics, EventFilterParams &filter,
                         std::string &error);

/// Tries to parse params and initialize event filter
bool InitializeEventFilter(const Json::Value &params, EventFilterParams &filter,
                           std::string &error);

/// Returns true if event filter matches given address and topics
bool Match(const EventFilterParams &filter, const Address &address,
           const std::vector<Quantity> &topics);

Json::Value CreateEventResponseItem(EpochNumber epoch, const TxnHash &tx_hash,
                                    const Address &address,
                                    const std::vector<Quantity> &topics,
                                    const Json::Value &data);

}  // namespace filters
}  // namespace evmproj

#endif  // ZILLIQA_SRC_LIBETH_FILTERS_FILTERUTILS_H_
