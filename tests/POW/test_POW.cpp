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
* 
* Test cases obtained from https://github.com/ethereum/ethash
**/

#include "libCrypto/Sha3.h"
#include <depends/libethash/ethash.h>
#include <depends/libethash/fnv.h>
#include <depends/libethash/internal.h>
#include <depends/libethash/io.h>
#include <iomanip>
#include <libPOW/pow.h>

#ifdef _WIN32
#include <Shlobj.h>
#include <windows.h>
#endif

#define BOOST_TEST_MODULE Daggerhashimoto
#define BOOST_TEST_MAIN

#include <boost/filesystem.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;
using byte = uint8_t;
using bytes = std::vector<byte>;
namespace fs = boost::filesystem;

// Just an alloca "wrapper" to silence uint64_t to size_t conversion warnings in windows
// consider replacing alloca calls with something better though!
#define our_alloca(param__) alloca((size_t)(param__))
#define UNUSED(x) (void)x
// some functions taken from eth::dev for convenience.
std::string bytesToHexString(const uint8_t* str, const uint64_t s)
{
    std::ostringstream ret;

    for (size_t i = 0; i < s; ++i)
        ret << std::hex << std::setfill('0') << std::setw(2) << std::nouppercase
            << (int)str[i];

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
        try
        {
            ret.push_back(fromHex(_s[s++]));
        }
        catch (...)
        {
            ret.push_back(0);
        }
    for (unsigned i = s; i < _s.size(); i += 2)
        try
        {
            ret.push_back((byte)(fromHex(_s[i]) * 16 + fromHex(_s[i + 1])));
        }
        catch (...)
        {
            ret.push_back(0);
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

BOOST_AUTO_TEST_CASE(fnv_hash_check)
{
    uint32_t x = 1235U;
    const uint32_t y = 9999999U, expected = (FNV_PRIME * x) ^ y;

    x = fnv_hash(x, y);

    BOOST_REQUIRE_MESSAGE(x == expected,
                          "\nexpected: " << expected << "\n"
                                         << "actual: " << x << "\n");
}

BOOST_AUTO_TEST_CASE(test_swap_endian32)
{
    uint32_t v32 = (uint32_t)0xBAADF00D;
    v32 = ethash_swap_u32(v32);
    BOOST_REQUIRE_EQUAL(v32, (uint32_t)0x0DF0ADBA);
}

BOOST_AUTO_TEST_CASE(test_swap_endian64)
{
    uint64_t v64 = (uint64_t)0xFEE1DEADDEADBEEF;
    v64 = ethash_swap_u64(v64);
    BOOST_REQUIRE_EQUAL(v64, (uint64_t)0xEFBEADDEADDEE1FE);
}

BOOST_AUTO_TEST_CASE(ethash_params_init_genesis_check)
{
    uint64_t full_size = ethash_get_datasize(0);
    uint64_t cache_size = ethash_get_cachesize(0);
    BOOST_REQUIRE_MESSAGE(full_size < ETHASH_DATASET_BYTES_INIT,
                          "\nfull size: " << full_size << "\n"
                                          << "should be less than or equal to: "
                                          << ETHASH_DATASET_BYTES_INIT << "\n");
    BOOST_REQUIRE_MESSAGE(
        full_size + 20 * ETHASH_MIX_BYTES >= ETHASH_DATASET_BYTES_INIT,
        "\nfull size + 20*MIX_BYTES: " << full_size + 20 * ETHASH_MIX_BYTES
                                       << "\n"
                                       << "should be greater than or equal to: "
                                       << ETHASH_DATASET_BYTES_INIT << "\n");
    BOOST_REQUIRE_MESSAGE(cache_size < ETHASH_DATASET_BYTES_INIT / 32,
                          "\ncache size: "
                              << cache_size << "\n"
                              << "should be less than or equal to: "
                              << ETHASH_DATASET_BYTES_INIT / 32 << "\n");
}

BOOST_AUTO_TEST_CASE(ethash_params_init_genesis_calcifide_check)
{
    uint64_t full_size = ethash_get_datasize(0);
    uint64_t cache_size = ethash_get_cachesize(0);
    const uint32_t expected_full_size = 1073739904;
    const uint32_t expected_cache_size = 16776896;
    BOOST_REQUIRE_MESSAGE(full_size == expected_full_size,
                          "\nexpected: " << expected_full_size << "\n"
                                         << "actual: " << full_size << "\n");
    BOOST_REQUIRE_MESSAGE(cache_size == expected_cache_size,
                          "\nexpected: " << expected_cache_size << "\n"
                                         << "actual: " << cache_size << "\n");
}

BOOST_AUTO_TEST_CASE(ethash_params_calcifide_check_30000)
{
    uint64_t full_size = ethash_get_datasize(30000);
    uint64_t cache_size = ethash_get_cachesize(30000);
    const uint32_t expected_full_size = 1082130304;
    const uint32_t expected_cache_size = 16907456;
    BOOST_REQUIRE_MESSAGE(full_size == expected_full_size,
                          "\nexpected: " << expected_full_size << "\n"
                                         << "actual: " << full_size << "\n");
    BOOST_REQUIRE_MESSAGE(cache_size == expected_cache_size,
                          "\nexpected: " << expected_cache_size << "\n"
                                         << "actual: " << cache_size << "\n");
}

BOOST_AUTO_TEST_CASE(ethash_check_difficulty_check)
{
    ethash_h256_t hash;
    ethash_h256_t target;
    memcpy(&hash, "11111111111111111111111111111111", 32);
    memcpy(&target, "22222222222222222222222222222222", 32);
    BOOST_REQUIRE_MESSAGE(
        ethash_check_difficulty(&hash, &target),
        "\nexpected \"" << std::string((char*)&hash, 32).c_str()
                        << "\" to have the same or less difficulty than \""
                        << std::string((char*)&target, 32).c_str() << "\"\n");
    BOOST_REQUIRE_MESSAGE(ethash_check_difficulty(&hash, &hash), "");
    // "\nexpected \"" << hash << "\" to have the same or less difficulty than \"" << hash << "\"\n");
    memcpy(&target, "11111111111111111111111111111112", 32);
    BOOST_REQUIRE_MESSAGE(ethash_check_difficulty(&hash, &target), "");
    // "\nexpected \"" << hash << "\" to have the same or less difficulty than \"" << target << "\"\n");
    memcpy(&target, "11111111111111111111111111111110", 32);
    BOOST_REQUIRE_MESSAGE(!ethash_check_difficulty(&hash, &target), "");
    // "\nexpected \"" << hash << "\" to have more difficulty than \"" << target << "\"\n");
}

BOOST_AUTO_TEST_CASE(test_ethash_io_mutable_name)
{
    char mutable_name[DAG_MUTABLE_NAME_MAX_SIZE];
    // should have at least 8 bytes provided since this is what we test :)
    ethash_h256_t seed1
        = ethash_h256_static_init(0, 10, 65, 255, 34, 55, 22, 8);
    ethash_io_mutable_name(1, &seed1, mutable_name);
    BOOST_REQUIRE_EQUAL(0, strcmp(mutable_name, "full-R1-000a41ff22371608"));
    ethash_h256_t seed2 = ethash_h256_static_init(0, 0, 0, 0, 0, 0, 0, 0);
    ethash_io_mutable_name(44, &seed2, mutable_name);
    BOOST_REQUIRE_EQUAL(0, strcmp(mutable_name, "full-R44-0000000000000000"));
}

BOOST_AUTO_TEST_CASE(test_ethash_dir_creation)
{
    ethash_h256_t seedhash;
    FILE* f = NULL;
    memset(&seedhash, 0, 32);
    BOOST_REQUIRE_EQUAL(
        ETHASH_IO_MEMO_MISMATCH,
        ethash_io_prepare("./test_ethash_directory/", seedhash, &f, 64, false));
    BOOST_REQUIRE(f);

    // let's make sure that the directory was created
    BOOST_REQUIRE(fs::is_directory(fs::path("./test_ethash_directory/")));

    // cleanup
    fclose(f);
    fs::remove_all("./test_ethash_directory/");
}

BOOST_AUTO_TEST_CASE(test_ethash_io_memo_file_match)
{
    uint64_t full_size;
    uint64_t cache_size;
    ethash_h256_t seed;
    ethash_h256_t hash;
    FILE* f;
    memcpy(&seed, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~", 32);
    memcpy(&hash, "~~~X~~~~~~~~~~~~~~~~~~~~~~~~~~~~", 32);

    cache_size = 1024;
    full_size = 1024 * 32;

    ethash_light_t light = ethash_light_new_internal(cache_size, &seed);
    ethash_full_t full = ethash_full_new_internal("./test_ethash_directory/",
                                                  seed, full_size, light, NULL);
    BOOST_ASSERT(full);
    // let's make sure that the directory was created
    BOOST_REQUIRE(fs::is_directory(fs::path("./test_ethash_directory/")));
    // delete the full here so that memory is properly unmapped and FILE handler freed
    ethash_full_delete(full);
    // and check that we have a match when checking again
    BOOST_REQUIRE_EQUAL(ETHASH_IO_MEMO_MATCH,
                        ethash_io_prepare("./test_ethash_directory/", seed, &f,
                                          full_size, false));
    BOOST_REQUIRE(f);

    // cleanup
    fclose(f);
    ethash_light_delete(light);
    fs::remove_all("./test_ethash_directory/");
}

BOOST_AUTO_TEST_CASE(test_ethash_io_memo_file_size_mismatch)
{
    static const int blockn = 0;
    ethash_h256_t seedhash = ethash_get_seedhash(blockn);
    FILE* f = NULL;
    BOOST_REQUIRE_EQUAL(
        ETHASH_IO_MEMO_MISMATCH,
        ethash_io_prepare("./test_ethash_directory/", seedhash, &f, 64, false));
    BOOST_REQUIRE(f);
    fclose(f);

    // let's make sure that the directory was created
    BOOST_REQUIRE(fs::is_directory(fs::path("./test_ethash_directory/")));
    // and check that we get the size mismatch detected if we request diffferent size
    BOOST_REQUIRE_EQUAL(
        ETHASH_IO_MEMO_SIZE_MISMATCH,
        ethash_io_prepare("./test_ethash_directory/", seedhash, &f, 65, false));

    // cleanup
    fs::remove_all("./test_ethash_directory/");
}

BOOST_AUTO_TEST_CASE(test_ethash_get_default_dirname)
{
    char result[256];
    // this is really not an easy thing to test for in a unit test
    // TODO: Improve this test ...
#ifdef _WIN32
    char homedir[256];
    BOOST_REQUIRE(SUCCEEDED(
        SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, (CHAR*)homedir)));
    BOOST_REQUIRE(ethash_get_default_dirname(result, 256));
    std::string res
        = std::string(homedir) + std::string("\\AppData\\Local\\Ethash\\");
#else
    char* homedir = getenv("HOME");
    BOOST_REQUIRE(ethash_get_default_dirname(result, 256));
    std::string res = std::string(homedir) + std::string("/.ethash/");
#endif
    BOOST_CHECK_MESSAGE(strcmp(res.c_str(), result) == 0,
                        "Expected \"" + res + "\" but got \""
                            + std::string(result) + "\"");
}

BOOST_AUTO_TEST_CASE(light_and_full_client_checks)
{
    uint64_t full_size;
    uint64_t cache_size;
    ethash_h256_t seed;
    ethash_h256_t hash;
    ethash_h256_t difficulty;
    ethash_return_value_t light_out;
    ethash_return_value_t full_out;
    memcpy(&seed, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~", 32);
    memcpy(&hash, "~~~X~~~~~~~~~~~~~~~~~~~~~~~~~~~~", 32);

    // Set the difficulty
    ethash_h256_set(&difficulty, 0, 197);
    ethash_h256_set(&difficulty, 1, 90);
    for (int i = 2; i < 32; i++)
        ethash_h256_set(&difficulty, i, 255);

    cache_size = 1024;
    full_size = 1024 * 32;

    ethash_light_t light = ethash_light_new_internal(cache_size, &seed);
    ethash_full_t full = ethash_full_new_internal("./test_ethash_directory/",
                                                  seed, full_size, light, NULL);
    BOOST_ASSERT(full);
    {
        const std::string expected
            = "2da2b506f21070e1143d908e867962486d6b0a02e31d468fd5e3a7143aafa76a"
              "14201f63374314e2a6aaf84ad2eb57105dea3378378965a1b3873453bb2b78f9"
              "a8620b2ebeca41fbc773bb837b5e724d6eb2de570d99858df0d7d97067fb8103"
              "b21757873b735097b35d3bea8fd1c359a9e8a63c1540c76c9784cf8d975e995c"
              "a8620b2ebeca41fbc773bb837b5e724d6eb2de570d99858df0d7d97067fb8103"
              "b21757873b735097b35d3bea8fd1c359a9e8a63c1540c76c9784cf8d975e995c"
              "a8620b2ebeca41fbc773bb837b5e724d6eb2de570d99858df0d7d97067fb8103"
              "b21757873b735097b35d3bea8fd1c359a9e8a63c1540c76c9784cf8d975e995c"
              "259440b89fa3481c2c33171477c305c8e1e421f8d8f6d59585449d0034f3e421"
              "808d8da6bbd0b6378f567647cc6c4ba6c434592b198ad444e7284905b7c6adaf"
              "70bf43ec2daa7bd5e8951aa609ab472c124cf9eba3d38cff5091dc3f58409edc"
              "c386c743c3bd66f92408796ee1e82dd149eaefbf52b00ce33014a6eb3e506254"
              "13b072a58bc01da28262f42cbe4f87d4abc2bf287d15618405a1fe4e386fcdaf"
              "bb171064bd99901d8f81dd6789396ce5e364ac944bbbd75a7827291c70b42d26"
              "385910cd53ca535ab29433dd5c5714d26e0dce95514c5ef866329c12e958097e"
              "84462197c2b32087849dab33e88b11da61d52f9dbc0b92cc61f742c07dbbf751"
              "c49d7678624ee60dfbe62e5e8c47a03d8247643f3d16ad8c8e663953bcda1f59"
              "d7e2d4a9bf0768e789432212621967a8f41121ad1df6ae1fa78782530695414c"
              "6213942865b2730375019105cae91a4c17a558d4b63059661d9f108362143107"
              "babe0b848de412e4da59168cce82bfbff3c99e022dd6ac1e559db991f2e3f7bb"
              "910cefd173e65ed00a8d5d416534e2c8416ff23977dbf3eb7180b75c71580d08"
              "ce95efeb9b0afe904ea12285a392aff0c8561ff79fca67f694a62b9e52377485"
              "c57cc3598d84cac0a9d27960de0cc31ff9bbfe455acaa62c8aa5d2cce96f345d"
              "a9afe843d258a99c4eaf3650fc62efd81c7b81cd0d534d2d71eeda7a6e315d54"
              "0b4473c80f8730037dc2ae3e47b986240cfc65ccc565f0d8cde0bc68a57e39a2"
              "71dda57440b3598bee19f799611d25731a96b5dbbbefdff6f4f6561614626330"
              "30d62560ea4e9c161cf78fc96a2ca5aaa32453a6c5dea206f766244e8c9d9a8d"
              "c61185ce37f1fc804459c5f07434f8ecb34141b8dcae7eae704c950b55556c5f"
              "40140c3714b45eddb02637513268778cbf937a33e4e33183685f9deb31ef54e9"
              "0161e76d969587dd782eaa94e289420e7c2ee908517f5893a26fdb5873d68f92"
              "d118d4bcf98d7a4916794d6ab290045e30f9ea00ca547c584b8482b0331ba153"
              "9a0f2714fddc3a0b06b0cfbb6a607b8339c39bcfd6640b1f653e9d70ef6c985"
              "b",
            actual = bytesToHexString((uint8_t const*)light->cache, cache_size);

        BOOST_REQUIRE_MESSAGE(expected == actual,
                              "\nexpected: " << expected.c_str() << "\n"
                                             << "actual: " << actual.c_str()
                                             << "\n");
    }
    {
        node node;
        ethash_calculate_dag_item(&node, 0, light);
        const std::string actual
            = bytesToHexString((uint8_t const*)&node, sizeof(node)),
            expected = "b1698f829f90b35455804e5185d78f549fcb1bdce2bee006d4d7e68"
                       "eb154b596be1427769eb1c3c3e93180c760af75f81d1023da6a0ffb"
                       "e321c153a7c0103597";
        BOOST_REQUIRE_MESSAGE(actual == expected,
                              "\n"
                                  << "expected: " << expected.c_str() << "\n"
                                  << "actual: " << actual.c_str() << "\n");
    }
    {
        for (uint32_t i = 0; i < full_size / sizeof(node); ++i)
        {
            for (uint32_t j = 0; j < 32; ++j)
            {
                node expected_node;
                ethash_calculate_dag_item(&expected_node, j, light);
                const std::string actual
                    = bytesToHexString((uint8_t const*)&(full->data[j]),
                                       sizeof(node)),
                    expected = bytesToHexString((uint8_t const*)&expected_node,
                                                sizeof(node));
                BOOST_REQUIRE_MESSAGE(
                    actual == expected,
                    "\ni: " << j << "\n"
                            << "expected: " << expected.c_str() << "\n"
                            << "actual: " << actual.c_str() << "\n");
            }
        }
    }
    {
        uint64_t nonce = 0x7c7c597c;
        full_out = ethash_full_compute(full, hash, nonce);
        BOOST_REQUIRE(full_out.success);
        light_out
            = ethash_light_compute_internal(light, full_size, hash, nonce);
        BOOST_REQUIRE(light_out.success);
        const std::string light_result_string
            = blockhashToHexString(&light_out.result),
            full_result_string = blockhashToHexString(&full_out.result);
        BOOST_REQUIRE_MESSAGE(
            light_result_string == full_result_string,
            "\nlight result: " << light_result_string.c_str() << "\n"
                               << "full result: " << full_result_string.c_str()
                               << "\n");
        const std::string light_mix_hash_string
            = blockhashToHexString(&light_out.mix_hash),
            full_mix_hash_string = blockhashToHexString(&full_out.mix_hash);
        BOOST_REQUIRE_MESSAGE(full_mix_hash_string == light_mix_hash_string,
                              "\nlight mix hash: "
                                  << light_mix_hash_string.c_str() << "\n"
                                  << "full mix hash: "
                                  << full_mix_hash_string.c_str() << "\n");
        ethash_h256_t check_hash;
        ethash_quick_hash(&check_hash, &hash, nonce, &full_out.mix_hash);
        const std::string check_hash_string = blockhashToHexString(&check_hash);
        BOOST_REQUIRE_MESSAGE(check_hash_string == full_result_string,
                              "\ncheck hash string: "
                                  << check_hash_string.c_str() << "\n"
                                  << "full result: "
                                  << full_result_string.c_str() << "\n");
    }
    {
        full_out = ethash_full_compute(full, hash, 5);
        BOOST_REQUIRE(full_out.success);
        std::string light_result_string
            = blockhashToHexString(&light_out.result),
            full_result_string = blockhashToHexString(&full_out.result);
        BOOST_REQUIRE_MESSAGE(light_result_string != full_result_string,
                              "\nlight result and full result should differ: "
                                  << light_result_string.c_str() << "\n");

        light_out = ethash_light_compute_internal(light, full_size, hash, 5);
        BOOST_REQUIRE(light_out.success);
        light_result_string = blockhashToHexString(&light_out.result);
        BOOST_REQUIRE_MESSAGE(
            light_result_string == full_result_string,
            "\nlight result and full result should be the same\n"
                << "light result: " << light_result_string.c_str() << "\n"
                << "full result: " << full_result_string.c_str() << "\n");
        std::string light_mix_hash_string
            = blockhashToHexString(&light_out.mix_hash),
            full_mix_hash_string = blockhashToHexString(&full_out.mix_hash);
        BOOST_REQUIRE_MESSAGE(full_mix_hash_string == light_mix_hash_string,
                              "\nlight mix hash: "
                                  << light_mix_hash_string.c_str() << "\n"
                                  << "full mix hash: "
                                  << full_mix_hash_string.c_str() << "\n");
        BOOST_REQUIRE_MESSAGE(
            ethash_check_difficulty(&full_out.result, &difficulty),
            "ethash_check_difficulty failed");
        BOOST_REQUIRE_MESSAGE(ethash_quick_check_difficulty(
                                  &hash, 5U, &full_out.mix_hash, &difficulty),
                              "ethash_quick_check_difficulty failed");
    }
    ethash_light_delete(light);
    ethash_full_delete(full);
    fs::remove_all("./test_ethash_directory/");
}

BOOST_AUTO_TEST_CASE(ethash_full_new_when_dag_exists_with_wrong_size)
{
    uint64_t full_size;
    uint64_t cache_size;
    ethash_h256_t seed;
    ethash_h256_t hash;
    ethash_return_value_t full_out;
    ethash_return_value_t light_out;
    memcpy(&seed, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~", 32);
    memcpy(&hash, "~~~X~~~~~~~~~~~~~~~~~~~~~~~~~~~~", 32);

    cache_size = 1024;
    full_size = 1024 * 32;

    // first make a DAG file of "wrong size"
    FILE* f;
    BOOST_REQUIRE_EQUAL(
        ETHASH_IO_MEMO_MISMATCH,
        ethash_io_prepare("./test_ethash_directory/", seed, &f, 64, false));
    fclose(f);

    // then create new DAG, which should detect the wrong size and force create a new file
    ethash_light_t light = ethash_light_new_internal(cache_size, &seed);
    BOOST_ASSERT(light);
    ethash_full_t full = ethash_full_new_internal("./test_ethash_directory/",
                                                  seed, full_size, light, NULL);
    BOOST_ASSERT(full);
    {
        uint64_t nonce = 0x7c7c597c;
        full_out = ethash_full_compute(full, hash, nonce);
        BOOST_REQUIRE(full_out.success);
        light_out
            = ethash_light_compute_internal(light, full_size, hash, nonce);
        BOOST_REQUIRE(light_out.success);
        const std::string light_result_string
            = blockhashToHexString(&light_out.result),
            full_result_string = blockhashToHexString(&full_out.result);
        BOOST_REQUIRE_MESSAGE(
            light_result_string == full_result_string,
            "\nlight result: " << light_result_string.c_str() << "\n"
                               << "full result: " << full_result_string.c_str()
                               << "\n");
        const std::string light_mix_hash_string
            = blockhashToHexString(&light_out.mix_hash),
            full_mix_hash_string = blockhashToHexString(&full_out.mix_hash);
        BOOST_REQUIRE_MESSAGE(full_mix_hash_string == light_mix_hash_string,
                              "\nlight mix hash: "
                                  << light_mix_hash_string.c_str() << "\n"
                                  << "full mix hash: "
                                  << full_mix_hash_string.c_str() << "\n");
        ethash_h256_t check_hash;
        ethash_quick_hash(&check_hash, &hash, nonce, &full_out.mix_hash);
        const std::string check_hash_string = blockhashToHexString(&check_hash);
        BOOST_REQUIRE_MESSAGE(check_hash_string == full_result_string,
                              "\ncheck hash string: "
                                  << check_hash_string.c_str() << "\n"
                                  << "full result: "
                                  << full_result_string.c_str() << "\n");
    }

    ethash_light_delete(light);
    ethash_full_delete(full);
    fs::remove_all("./test_ethash_directory/");
}

static bool g_executed = false;
static unsigned g_prev_progress = 0;
static int test_full_callback(unsigned _progress)
{
    g_executed = true;
    BOOST_CHECK(_progress >= g_prev_progress);
    g_prev_progress = _progress;
    return 0;
}

static int test_full_callback_that_fails(unsigned _progress) { return 1; }

static int test_full_callback_create_incomplete_dag(unsigned _progress)
{
    if (_progress >= 30)
    {
        return 1;
    }
    return 0;
}

BOOST_AUTO_TEST_CASE(full_client_callback)
{
    uint64_t full_size;
    uint64_t cache_size;
    ethash_h256_t seed;
    ethash_h256_t hash;
    memcpy(&seed, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~", 32);
    memcpy(&hash, "~~~X~~~~~~~~~~~~~~~~~~~~~~~~~~~~", 32);

    cache_size = 1024;
    full_size = 1024 * 32;

    ethash_light_t light = ethash_light_new_internal(cache_size, &seed);
    ethash_full_t full = ethash_full_new_internal(
        "./test_ethash_directory/", seed, full_size, light, test_full_callback);
    BOOST_ASSERT(full);
    BOOST_CHECK(g_executed);
    BOOST_REQUIRE_EQUAL(g_prev_progress, 100);

    ethash_full_delete(full);
    ethash_light_delete(light);
    fs::remove_all("./test_ethash_directory/");
}

BOOST_AUTO_TEST_CASE(failing_full_client_callback)
{
    uint64_t full_size;
    uint64_t cache_size;
    ethash_h256_t seed;
    ethash_h256_t hash;
    memcpy(&seed, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~", 32);
    memcpy(&hash, "~~~X~~~~~~~~~~~~~~~~~~~~~~~~~~~~", 32);

    cache_size = 1024;
    full_size = 1024 * 32;

    ethash_light_t light = ethash_light_new_internal(cache_size, &seed);
    ethash_full_t full
        = ethash_full_new_internal("./test_ethash_directory/", seed, full_size,
                                   light, test_full_callback_that_fails);
    BOOST_ASSERT(!full);
    UNUSED(full);
    ethash_light_delete(light);
    fs::remove_all("./test_ethash_directory/");
}

BOOST_AUTO_TEST_CASE(test_incomplete_dag_file)
{
    uint64_t full_size;
    uint64_t cache_size;
    ethash_h256_t seed;
    ethash_h256_t hash;
    memcpy(&seed, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~", 32);
    memcpy(&hash, "~~~X~~~~~~~~~~~~~~~~~~~~~~~~~~~~", 32);

    cache_size = 1024;
    full_size = 1024 * 32;

    ethash_light_t light = ethash_light_new_internal(cache_size, &seed);
    // create a full but stop at 30%, so no magic number is written
    ethash_full_t full = ethash_full_new_internal(
        "./test_ethash_directory/", seed, full_size, light,
        test_full_callback_create_incomplete_dag);
    BOOST_ASSERT(!full);
    UNUSED(full);
    FILE* f = NULL;
    // confirm that we get a size_mismatch because the magic number is missing
    BOOST_REQUIRE_EQUAL(ETHASH_IO_MEMO_SIZE_MISMATCH,
                        ethash_io_prepare("./test_ethash_directory/", seed, &f,
                                          full_size, false));
    ethash_light_delete(light);
    fs::remove_all("./test_ethash_directory/");
}

BOOST_AUTO_TEST_CASE(test_block22_verification)
{
    // from POC-9 testnet, epoch 0
    ethash_light_t light = ethash_light_new(22);
    ethash_h256_t seedhash = stringToBlockhash(
        "372eca2454ead349c3df0ab5d00b0b706b23e49d469387db91811cee0358fc6d");
    BOOST_ASSERT(light);
    ethash_return_value_t ret
        = ethash_light_compute(light, seedhash, 0x495732e0ed7a801cU);
    BOOST_REQUIRE_EQUAL(
        blockhashToHexString(&ret.result),
        "00000b184f1fdd88bfd94c86c39e65db0c36144d5e43f745f722196e730cb614");
    ethash_h256_t difficulty = ethash_h256_static_init(0x2, 0x5, 0x40);
    BOOST_REQUIRE(ethash_check_difficulty(&ret.result, &difficulty));
    ethash_light_delete(light);
}

BOOST_AUTO_TEST_CASE(test_block30001_verification)
{
    // from POC-9 testnet, epoch 1
    ethash_light_t light = ethash_light_new(30001);
    ethash_h256_t seedhash = stringToBlockhash(
        "7e44356ee3441623bc72a683fd3708fdf75e971bbe294f33e539eedad4b92b34");
    BOOST_ASSERT(light);
    ethash_return_value_t ret
        = ethash_light_compute(light, seedhash, 0x318df1c8adef7e5eU);
    ethash_h256_t difficulty = ethash_h256_static_init(0x17, 0x62, 0xff);
    BOOST_REQUIRE(ethash_check_difficulty(&ret.result, &difficulty));
    ethash_light_delete(light);
}

BOOST_AUTO_TEST_CASE(test_block60000_verification)
{
    // from POC-9 testnet, epoch 2
    ethash_light_t light = ethash_light_new(60000);
    ethash_h256_t seedhash = stringToBlockhash(
        "5fc898f16035bf5ac9c6d9077ae1e3d5fc1ecc3c9fd5bee8bb00e810fdacbaa0");
    BOOST_ASSERT(light);
    ethash_return_value_t ret
        = ethash_light_compute(light, seedhash, 0x50377003e5d830caU);
    ethash_h256_t difficulty = ethash_h256_static_init(0x25, 0xa6, 0x1e);
    BOOST_REQUIRE(ethash_check_difficulty(&ret.result, &difficulty));
    ethash_light_delete(light);
}

BOOST_AUTO_TEST_CASE(mining_and_verification)
{
    POW& POWClient = POW::GetInstance();
    std::array<unsigned char, 32> rand1 = {{'0', '1'}};
    std::array<unsigned char, 32> rand2 = {{'0', '2'}};
    boost::multiprecision::uint128_t ipAddr = 2307193356;
    PubKey pubKey = Schnorr::GetInstance().GenKeyPair().second;

    // Light client mine and verify
    uint8_t difficultyToUse = 10;
    uint8_t blockToUse = 0;
    ethash_mining_result_t winning_result = POWClient.PoWMine(
        blockToUse, difficultyToUse, rand1, rand2, ipAddr, pubKey, false);
    bool verifyLight
        = POWClient.PoWVerify(blockToUse, difficultyToUse, rand1, rand2, ipAddr,
                              pubKey, false, winning_result.winning_nonce,
                              winning_result.result, winning_result.mix_hash);
    BOOST_REQUIRE(verifyLight);

    // Full client mine and verify
    winning_result = POWClient.PoWMine(blockToUse, difficultyToUse, rand1,
                                       rand2, ipAddr, pubKey, true);
    bool verifyFull
        = POWClient.PoWVerify(blockToUse, difficultyToUse, rand1, rand2, ipAddr,
                              pubKey, true, winning_result.winning_nonce,
                              winning_result.result, winning_result.mix_hash);
    BOOST_REQUIRE(verifyFull);

    // Full client mine and light client verify
    winning_result = POWClient.PoWMine(blockToUse, difficultyToUse, rand1,
                                       rand2, ipAddr, pubKey, true);
    bool verifyFullMineLightVerify
        = POWClient.PoWVerify(blockToUse, difficultyToUse, rand1, rand2, ipAddr,
                              pubKey, false, winning_result.winning_nonce,
                              winning_result.result, winning_result.mix_hash);
    BOOST_REQUIRE(verifyFullMineLightVerify);
}

BOOST_AUTO_TEST_CASE(mining_and_verification_wrong_inputs)
{
    //expect to fail test cases
    uint8_t difficultyToUse = 10;
    uint8_t blockToUse = 0;
    POW& POWClient = POW::GetInstance();
    std::array<unsigned char, 32> rand1 = {{'0', '1'}};
    std::array<unsigned char, 32> rand2 = {{'0', '2'}};
    boost::multiprecision::uint128_t ipAddr = 2307193356;
    PubKey pubKey = Schnorr::GetInstance().GenKeyPair().second;

    ethash_mining_result_t winning_result = POWClient.PoWMine(
        blockToUse, difficultyToUse, rand1, rand2, ipAddr, pubKey, true);
    rand1 = {{'0', '3'}};
    bool verifyFullMineLightVerify
        = POWClient.PoWVerify(blockToUse, difficultyToUse, rand1, rand2, ipAddr,
                              pubKey, false, winning_result.winning_nonce,
                              winning_result.result, winning_result.mix_hash);
    BOOST_REQUIRE(!verifyFullMineLightVerify);
}

BOOST_AUTO_TEST_CASE(mining_and_verification_wrong_difficulty)
{
    //expect to fail test cases
    uint8_t difficultyToUse = 10;
    uint8_t blockToUse = 0;
    POW& POWClient = POW::GetInstance();
    std::array<unsigned char, 32> rand1 = {{'0', '1'}};
    std::array<unsigned char, 32> rand2 = {{'0', '2'}};
    boost::multiprecision::uint128_t ipAddr = 2307193356;
    PubKey pubKey = Schnorr::GetInstance().GenKeyPair().second;

    ethash_mining_result_t winning_result = POWClient.PoWMine(
        blockToUse, difficultyToUse, rand1, rand2, ipAddr, pubKey, true);

    // Now let's adjust the difficulty expectation during verification
    difficultyToUse = 30;
    bool verifyFullMineLightVerify
        = POWClient.PoWVerify(blockToUse, difficultyToUse, rand1, rand2, ipAddr,
                              pubKey, false, winning_result.winning_nonce,
                              winning_result.result, winning_result.mix_hash);
    BOOST_REQUIRE(!verifyFullMineLightVerify);
}

BOOST_AUTO_TEST_CASE(mining_and_verification_different_wrong_winning_nonce)
{
    //expect to fail test cases
    uint8_t difficultyToUse = 10;
    uint8_t blockToUse = 0;
    POW& POWClient = POW::GetInstance();
    std::array<unsigned char, 32> rand1 = {{'0', '1'}};
    std::array<unsigned char, 32> rand2 = {{'0', '2'}};
    boost::multiprecision::uint128_t ipAddr = 2307193356;
    PubKey pubKey = Schnorr::GetInstance().GenKeyPair().second;

    ethash_mining_result_t winning_result = POWClient.PoWMine(
        blockToUse, difficultyToUse, rand1, rand2, ipAddr, pubKey, true);
    uint64_t winning_nonce = 0;
    bool verifyFullMineLightVerify = POWClient.PoWVerify(
        blockToUse, difficultyToUse, rand1, rand2, ipAddr, pubKey, false,
        winning_nonce, winning_result.result, winning_result.mix_hash);
    BOOST_REQUIRE(!verifyFullMineLightVerify);
}

// Test of Full DAG creation with the minimal ethash.h API.
// Commented out since travis tests would take too much time.
// Uncomment and run on your own machine if you want to confirm
// it works fine.
#if 0
static int progress_cb(unsigned _progress)
{
    printf("CREATING DAG. PROGRESS: %u\n", _progress);
    fflush(stdout);
    return 0;
}

BOOST_AUTO_TEST_CASE(full_dag_test)
{
    ethash_light_t light = ethash_light_new(55);
    BOOST_ASSERT(light);
    ethash_full_t full = ethash_full_new(light, progress_cb);
    BOOST_ASSERT(full);
    ethash_light_delete(light);
    ethash_full_delete(full);
}
#endif