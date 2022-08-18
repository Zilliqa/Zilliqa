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

#ifndef ZILLIQA_ACCOUNTSTORECOMMON_HPP
#define ZILLIQA_ACCOUNTSTORECOMMON_HPP
// Once we move to C++ 17 then this can be upgraded to std:variant

class Account;

enum map_type {
  MAP = 0,
  UNORDERED_MAP = 1
} ;

class acMapClass {
 public:
  acMapClass() = delete;

  acMapClass( const std::shared_ptr<std::map<Address,Account>>& init)
      :m_type(map_type::MAP),m_map(init) {
  }

  acMapClass( const std::shared_ptr<std::unordered_map<Address,Account>>& init)
      :m_type(map_type::UNORDERED_MAP),m_unordered_map(init) {
  }

  const std::shared_ptr<std::unordered_map<Address,Account>>
  GetUMap() const {
    return m_unordered_map;
  }

  const std::shared_ptr<std::map<Address,Account>>
  GetMap() const {
    return m_map;
  }

  const map_type& GetType() const{
    return m_type;
  }

 private:
  map_type                                                m_type;
  std::shared_ptr<std::map<Address,Account>>              m_map;
  std::shared_ptr<std::unordered_map<Address,Account>>    m_unordered_map;
};

#endif  // ZILLIQA_ACCOUNTSTORECOMMON_HPP
