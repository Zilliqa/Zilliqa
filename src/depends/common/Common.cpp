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
#include "Common.h"
#include "libUtils/Logger.h"

using namespace std;

namespace dev
{
    bytes const NullBytes;
    u256 const Invalid256 = ~(u256) 0;
    u128 const Invalid128 = ~(u128) 0;
    std::string const EmptyString;

    uint64_t utcTime()
    {
        // TODO: Fix if possible to not use time(0) and merge only after testing in all platforms
        // time_t t = time(0);
        // return mktime(gmtime(&t));
        return time(0);
    }
}