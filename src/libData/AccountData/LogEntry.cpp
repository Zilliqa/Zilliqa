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

#include "LogEntry.h"

using namespace std;

bool LogEntry::Install(const Json::Value& eventObj,
                       const Address& address)  //, unsigned int& numIndexed)
{
  m_eventObj = eventObj;
  if (!m_eventObj.isMember("_eventname") || !m_eventObj.isMember("params")) {
    LOG_GENERAL(WARNING,
                "Address: " << address.hex()
                            << ", The json object of events is corrupted");
    return false;
  }

  for (auto& p : m_eventObj["params"]) {
    if (!p.isMember("vname") || !p.isMember("type") ||
        !p.isMember("value"))  // || !p.isMember("indexed"))
    {
      LOG_GENERAL(WARNING, "Address: " << address.hex() << " EventName: "
                                       << m_eventObj["_eventname"].asString()
                                       << ", The params is corrupted");
      return false;
    }
  }

  m_eventObj["address"] = "0x" + address.hex();
  return true;
}
