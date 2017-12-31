/**
* Copyright (c) 2017 Zilliqa 
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
*
* Test cases obtained from https://www.di-mgt.com.au/sha_testvectors.html
**/


#include <iomanip>
#include "libCrypto/sha3-fips.h"
#include "depends/libethash/ethash.h"

#ifdef _WIN32
#include <windows.h
#include <Shlobj.h>
#endif

#define BOOST_TEST_MODULE Daggerhashimoto
#define BOOST_TEST_MAIN

#include <iostream>
#include <fstream>
#include <sstream>

#include <vector>
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;
using byte = uint8_t;
using bytes = std::vector<byte>;
namespace fs = boost::filesystem;

// Just an alloca "wrapper" to silence uint64_t to size_t conversion warnings in windows
// consider replacing alloca calls with something better though!
#define our_alloca(param__) alloca((size_t)(param__))


// some functions taken from eth::dev for convenience.
std::string bytesToHexString(const uint8_t *str, const uint64_t s)
{
	std::ostringstream ret;

	for (size_t i = 0; i < s; ++i)
		ret << std::hex << std::setfill('0') << std::setw(2) << std::nouppercase << (int) str[i];

	return ret.str();
}

std::string blockhashToHexString(ethash_h256_t* _hash)
{
	return bytesToHexString((uint8_t*)_hash, 32);
}

int fromHex(char _i)
{
	if (_i >= '0' && _i <= '9')
		return _i - '0';
	if (_i >= 'a' && _i <= 'f')
		return _i - 'a' + 10;
	if (_i >= 'A' && _i <= 'F')
		return _i - 'A' + 10;

	BOOST_REQUIRE_MESSAGE(false, "should never get here");
	return -1;
}

bytes hexStringToBytes(std::string const& _s)
{
	unsigned s = (_s[0] == '0' && _s[1] == 'x') ? 2 : 0;
	std::vector<uint8_t> ret;
	ret.reserve((_s.size() - s + 1) / 2);

	if (_s.size() % 2)
	{
		try
		{
			ret.push_back(fromHex(_s[s++]));
		}
		catch (...)
		{
			ret.push_back(0);
		}
	}
	for (unsigned i = s; i < _s.size(); i += 2)
	{
		try
		{
			ret.push_back((byte)(fromHex(_s[i]) * 16 + fromHex(_s[i + 1])));
		}
		catch (...){
			ret.push_back(0);
		}
	}

	return ret;
}

ethash_h256_t stringToBlockhash(std::string const& _s)
{
	ethash_h256_t ret;
	bytes b = hexStringToBytes(_s);
	memcpy(&ret, b.data(), b.size());
	return ret;
}

BOOST_AUTO_TEST_CASE(SHA256_check_0bits) 
{
	ethash_h256_t input;
	ethash_h256_t out;
	memcpy(&input, "", 0);
	SHA3_256(&out, (uint8_t*)&input, 0);
	const std::string
	expected = "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a",
	actual = bytesToHexString((uint8_t*)&out, 32);
	BOOST_REQUIRE_MESSAGE(expected == actual,
		"\nexpected: " << expected.c_str() << "\n"
		<< "actual: " << actual.c_str() << "\n");
}

BOOST_AUTO_TEST_CASE(SHA256_check_24bits) 
{
	ethash_h256_t input;
	ethash_h256_t out;
	memcpy(&input, "abc", 3);
	SHA3_256(&out, (uint8_t*)&input, 3);
	const std::string
	expected = "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532",
	actual = bytesToHexString((uint8_t*)&out, 32);
	BOOST_REQUIRE_MESSAGE(expected == actual,
		"\nexpected: " << expected.c_str() << "\n"
		<< "actual: " << actual.c_str() << "\n");
}

BOOST_AUTO_TEST_CASE(SHA256_check_448bits) 
{
	ethash_h256_t input;
	ethash_h256_t out;
	memcpy(&input, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 448/8);
	SHA3_256(&out, (uint8_t*)&input, 448/8);
	const std::string
	expected = "41c0dba2a9d6240849100376a8235e2c82e1b9998a999e21db32dd97496d3376",
	actual = bytesToHexString((uint8_t*)&out, 32);
	BOOST_REQUIRE_MESSAGE(expected == actual,
		"\nexpected: " << expected.c_str() << "\n"
		<< "actual: " << actual.c_str() << "\n");
}


BOOST_AUTO_TEST_CASE(SHA256_check_896bits) 
{
	ethash_h256_t input;
	ethash_h256_t out;
	memcpy(&input, "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", 896/8);
	SHA3_256(&out, (uint8_t*)&input, 896/8);
	const std::string
	expected = "916f6061fe879741ca6469b43971dfdb28b1a32dc36cb3254e812be27aad1d18",
	actual = bytesToHexString((uint8_t*)&out, 32);
	BOOST_REQUIRE_MESSAGE(expected == actual,
		"\nexpected: " << expected.c_str() << "\n"
		<< "actual: " << actual.c_str() << "\n");
}


BOOST_AUTO_TEST_CASE(SHA512_check_0bits) 
{
	uint8_t input[64], out[64];
	memcpy(input, "", 0);
	SHA3_512(out, input, 0);
	const std::string
	expected = "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26",
	actual = bytesToHexString(out, 64);
	BOOST_REQUIRE_MESSAGE(expected == actual,
		"\nexpected: " << expected.c_str() << "\n"
		<< "actual: " << actual.c_str() << "\n");
}

BOOST_AUTO_TEST_CASE(SHA512_check_24bits) 
{
	uint8_t input[64], out[64];
	memcpy(input, "abc", 3);
	SHA3_512(out, input, 3);
	const std::string
	expected = "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0",
	actual = bytesToHexString(out, 64);
	BOOST_REQUIRE_MESSAGE(expected == actual,
		"\nexpected: " << expected.c_str() << "\n"
		<< "actual: " << actual.c_str() << "\n");
}


BOOST_AUTO_TEST_CASE(SHA512_check_448bits) 
{
	uint8_t input[64], out[64];
	memcpy(input, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 448/8);
	SHA3_512(out, input, 448/8);
	const std::string
	expected = "04a371e84ecfb5b8b77cb48610fca8182dd457ce6f326a0fd3d7ec2f1e91636dee691fbe0c985302ba1b0d8dc78c086346b533b49c030d99a27daf1139d6e75e",
	actual = bytesToHexString(out, 64);
	BOOST_REQUIRE_MESSAGE(expected == actual,
		"\nexpected: " << expected.c_str() << "\n"
		<< "actual: " << actual.c_str() << "\n");
}

BOOST_AUTO_TEST_CASE(SHA512_check_896bits) 
{
	uint8_t input[64], out[64];
	memcpy(input, "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", 896/8);
	SHA3_512(out, input, 896/8);
	const std::string
	expected = "afebb2ef542e6579c50cad06d2e578f9f8dd6881d7dc824d26360feebf18a4fa73e3261122948efcfd492e74e82e2189ed0fb440d187f382270cb455f21dd185",
	actual = bytesToHexString(out, 64);
	BOOST_REQUIRE_MESSAGE(expected == actual,
		"\nexpected: " << expected.c_str() << "\n"
		<< "actual: " << actual.c_str() << "\n");
}

