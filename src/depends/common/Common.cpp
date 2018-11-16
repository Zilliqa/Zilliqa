/**
* Copyright (c) 2018 Zilliqa 
* This is an alpha (internal) release and is not suitable for production.
**/

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