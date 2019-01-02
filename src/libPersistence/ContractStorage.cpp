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

#include "ContractStorage.h"

#include "libUtils/DataConversion.h"

using namespace dev;

bool ContractStorage::PutContractCode(const h160& address, const bytes& code) {
  return m_codeDB.Insert(address.hex(), code) == 0;
}

const bytes ContractStorage::GetContractCode(const h160& address) {
  return DataConversion::StringToCharArray(m_codeDB.Lookup(address.hex()));
}
