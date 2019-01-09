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

#ifndef __LOGENTRY_H__
#define __LOGENTRY_H__

#include <json/json.h>
#include "Address.h"

class LogEntry {
  Json::Value m_eventObj;
  // unsigned int m_numIndexed;

 public:
  LogEntry() = default;
  bool Install(const Json::Value& eventObj,
               const Address& address);  //, unsigned int& numIndexed);
  const Json::Value& GetJsonObject() const { return m_eventObj; }
};

#endif  // __LOGENTRY_H__
