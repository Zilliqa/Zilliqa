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

#include <depends/libethash/ethash.h>
#include <depends/libethash/fnv.h>
#include <depends/libethash/internal.h>
#include <depends/libethash/io.h>
#include <iomanip>
#include <libCrypto/sha3-fips.h>
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

BOOST_AUTO_TEST_CASE(SHA256_check)
{
    ethash_h256_t input;
    ethash_h256_t out;
    memcpy(&input, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~", 32);
    SHA3_256(&out, (uint8_t*)&input, 32);
    const std::string expected
        = "e4d017634c4c616698b0321147f574c3a1f08931432b80a136bb1b2bf9dd2704",
        actual = bytesToHexString((uint8_t*)&out, 32);
    BOOST_REQUIRE_MESSAGE(expected == actual,
                          "\nexpected: " << expected.c_str() << "\n"
                                         << "actual: " << actual.c_str()
                                         << "\n");
}

BOOST_AUTO_TEST_CASE(SHA512_check)
{
    uint8_t input[64], out[64];
    memcpy(input,
           "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
           64);
    SHA3_512(out, input, 64);
    const std::string expected = "049f29858ba95562f4ab77ac244988bdc8c35a6608442"
                                 "f6780c9b5eb843126778cd8fa8acba60255bc0865ed2b"
                                 "102424391502cfbdda00de65fa6cef134905c7",
                      actual = bytesToHexString(out, 64);
    BOOST_REQUIRE_MESSAGE(expected == actual,
                          "\nexpected: " << expected.c_str() << "\n"
                                         << "actual: " << actual.c_str()
                                         << "\n");
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
            = "660aed482dd5f7d93f68f86a4b0a3c921e269bc1c6d193b10be4faa694ffe949"
              "b0311f73da422099ceba8693a815a16dfd827ef1aec4d1faa9994ccebc2f2137"
              "9ea9f35afe48f5d1ec1c052efd8cff2739de349c993a5f9b5637c40db025730f"
              "2e43831f836b76d9559f3d1e4cad03b6329206bcc61fb0284298df8ccd778a70"
              "56882915a7f0d8b87cf4819f6cdba6a45cb9783cea6ea9d148696fea87463a9d"
              "29d4428081a0f6496f83e3be3ced989bcd1f7e1b1b06a7e01671b05008ed4148"
              "742d21e6dfc8b941dbabce4931c5334192e743fb09418b87dff662e1c48e41a3"
              "66ff4e1d02be80a58a398b59463cc30da751a95a955b74e778dd8f801d55a6c6"
              "435ded80f62e06424f583dca89f21cefa1021eb229f26234c72fb6ed7dcc2e8e"
              "52fa29b254e71cbce5afea9d185789e441ed8f7a58e82e1d9b29fe9eb78b73ab"
              "243d92f5a1328a4cc9f4cb6da60ee6f7b362472f7ad4fc117e3646c85061574c"
              "12e110bdfcd98d90f0d19b6bff5b44a7c69da1975c3a8522095eb9217e553c28"
              "3f55a095f5074ee6dd2ff3d0cd84a4624ce84bb2a09ac0c06e0e18b245b798a3"
              "4271196adb06d910bd55daf50fc1cf4e1a310cd08cc8a0b2f04e112d193601e8"
              "243d92f5a1328a4cc9f4cb6da60ee6f7b362472f7ad4fc117e3646c85061574c"
              "12e110bdfcd98d90f0d19b6bff5b44a7c69da1975c3a8522095eb9217e553c28"
              "aca9dd17029c38cd4942d0e94e6bb190188eae5b1d1969d4721438292e13ff5d"
              "423c3c8e97fac72e4c693ea27b8744b62a98dc284028c3c36c0ea633a838f810"
              "159e8ca8ac140fc5e44f3b9c8d59bbfd5916d0abc67d09f97b214b63e36327e9"
              "7e8f43e4dc4968ca407f513b0aed82c793937da5d5535b80e8d4c3f9dba03ca2"
              "2d771f339423b122a0832eb1087d88b478bf0436693809c9576b7aa58a7d3aa6"
              "f865daafd04b05ff25da3e45d6a9096c4e91b964e7fe2869dde0fae58629b7a7"
              "d578ab18da92def6cd49d83660bd95c70d2323990bdb2def02b97edc35fde0c7"
              "d3146970b4ea28678f79139c2e515797631d355ff32d58196c98305eb4a837c7"
              "5b8b40c8aab98e47010dac6b51959c3d141baac23bbcfdda971e3cbe8d2e3932"
              "e48eaac5251c8049b4164b76039d491fc22856214439ecfde946ecc990ca101a"
              "f83e2918a416e3092ec229fb32a92e507428ddb462d22b4d3ebeb914efd61935"
              "82b54de773fb939a73e995faf71e802dcc12ba1c9e40610f8e5c42f1e9056251"
              "77917365a0c1e8de501ea8ade0c1f4d5daaaa56dd9268c339eaa1243dd428415"
              "240f636540da330a70e8ec0b07969685cb60941310a981af763abaf93928ec04"
              "63fcf77b7f08bba4d3247064f0bdf5ede97f1aee17ee7c819988028b80471a71"
              "daa66b353331c2c747eb165c72a0b560313e3ed6b14de2b4317803134f9a0d8"
              "a",
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
            expected = "f8b7356f46e392184c3e9067629a8a7f8ee2c1bb4a5692ab0bd49c6"
                       "6d5423a87297ec05fb7662d9150f89d5ade6fdf974ac2d417753299"
                       "0d17802c5695950e7d";

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
    FILE* f = NULL;
    // confirm that we get a size_mismatch because the magic number is missing
    BOOST_REQUIRE_EQUAL(ETHASH_IO_MEMO_SIZE_MISMATCH,
                        ethash_io_prepare("./test_ethash_directory/", seed, &f,
                                          full_size, false));
    ethash_light_delete(light);
    fs::remove_all("./test_ethash_directory/");
}

BOOST_AUTO_TEST_CASE(seedhash_generation)
{
    ethash_h256_t seedhash = ethash_get_seedhash(0);
    std::string hash = bytesToHexString((uint8_t const*)&seedhash, 32);
    BOOST_REQUIRE_EQUAL(
        hash,
        "0000000000000000000000000000000000000000000000000000000000000000");

    seedhash = ethash_get_seedhash(30000);
    hash = bytesToHexString((uint8_t const*)&seedhash, 32);
    BOOST_REQUIRE_EQUAL(
        hash,
        "9e6291970cb44dd94008c79bcaf9d86f18b4b49ba5b2a04781db7199ed3b9e4e");
}

BOOST_AUTO_TEST_CASE(mining_and_verification)
{
    POW POWClient;
    std::array<unsigned char, 32> rand1 = {'0', '1'};
    std::array<unsigned char, 32> rand2 = {'0', '2'};
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
    POW POWClient;
    std::array<unsigned char, 32> rand1 = {'0', '1'};
    std::array<unsigned char, 32> rand2 = {'0', '2'};
    boost::multiprecision::uint128_t ipAddr = 2307193356;
    PubKey pubKey = Schnorr::GetInstance().GenKeyPair().second;

    ethash_mining_result_t winning_result = POWClient.PoWMine(
        blockToUse, difficultyToUse, rand1, rand2, ipAddr, pubKey, true);
    rand1 = {'0', '3'};
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
    POW POWClient;
    std::array<unsigned char, 32> rand1 = {'0', '1'};
    std::array<unsigned char, 32> rand2 = {'0', '2'};
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
    POW POWClient;
    std::array<unsigned char, 32> rand1 = {'0', '1'};
    std::array<unsigned char, 32> rand2 = {'0', '2'};
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
#if 1
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
