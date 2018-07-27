/**
* Copyright (c) 2018 Zilliqa
* This source code is being disclosed to you solely for the purpose of your participation in
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to
* the protocols and algorithms that are programmed into, and intended by, the code. You may
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd.,
* including modifying or publishing the code (or any part of it), and developing or forming
* another public or private blockchain network. This source code is provided ‘as is’ and no
* warranties are given as to title or non-infringement, merchantability or fitness for purpose
* and, to the extent permitted by law, all liability for your use of the code is disclaimed.
* Some programs in this code are governed by the GNU General Public License v3.0 (available at
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends
* and which include a reference to GPLv3 in their program files.
**/

#ifndef __SafeMath_H__
#define __SafeMath_H__

#include "Logger.h"
#include <boost/multiprecision/cpp_int.hpp>

class SafeMath
{
public:
    static bool mul(const boost::multiprecision::uint256_t& a,
                    const boost::multiprecision::uint256_t& b,
                    boost::multiprecision::uint256_t& result)
    {
        if (a == 0)
        {
            result = 0;
            return true;
        }

        boost::multiprecision::uint256_t c = a * b;
        if (c / a != b)
        {
            LOG_GENERAL(WARNING, "Multiplication Overflow!");
            return false;
        }
        result = c;
        return true;
    }

    static bool div(const boost::multiprecision::uint256_t& a,
                    const boost::multiprecision::uint256_t& b,
                    boost::multiprecision::uint256_t& result)
    {
        if (b <= 0)
        {
            LOG_GENERAL(WARNING, "Denominator cannot be zero!");
            return false;
        }

        boost::multiprecision::uint256_t c = a / b;
        if (a != b * c + a % b)
        {
            return false;
        }

        result = c;
        return true;
    }

    static bool sub(const boost::multiprecision::uint256_t& a,
                    const boost::multiprecision::uint256_t& b,
                    boost::multiprecision::uint256_t& result)
    {
        if (b > a)
        {
            LOG_GENERAL(WARNING, "Invalid Subtraction for Unsigned Integer!");
            return false;
        }

        result = a - b;
        return true;
    }

    static bool add(const boost::multiprecision::uint256_t& a,
                    const boost::multiprecision::uint256_t& b,
                    boost::multiprecision::uint256_t& result)
    {
        boost::multiprecision::uint256_t c = a + b;
        if (c - a != b)
        {
            LOG_GENERAL(WARNING, "Addition Overflow!");
            return false;
        }

        result = c;
        return true;
    }
};

#endif // __OVERFLOWSAFEOPS_H__
