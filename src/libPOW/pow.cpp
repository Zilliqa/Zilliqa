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

#include <ctime>
#include <iomanip>
#include <iostream>

#include "pow.h"
//#include "libCrypto/Sha3.h"
#include "common/Serializable.h"
#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"

POW::POW()
{
    currentBlockNum = 0;
    ethash_light_client = EthashLightNew(
        0); // TODO: Do we still need this? Can we call it at mediator?
}

POW::~POW() { EthashLightDelete(ethash_light_client); }

POW& POW::GetInstance()
{
    static POW pow;
    return pow;
}

void POW::StopMining() { shouldMine = false; }

std::string POW::BytesToHexString(const uint8_t* str, const uint64_t s)
{
    std::ostringstream ret;

    for (size_t i = 0; i < s; ++i)
        ret << std::hex << std::setfill('0') << std::setw(2) << std::nouppercase
            << (int)str[i];

    return ret.str();
}

std::vector<uint8_t> POW::HexStringToBytes(std::string const& _s)
{
    unsigned s = (_s[0] == '0' && _s[1] == 'x') ? 2 : 0;
    std::vector<uint8_t> ret;
    ret.reserve((_s.size() - s + 1) / 2);

    if (_s.size() % 2)
        try
        {
            ret.push_back(FromHex(_s[s++]));
        }
        catch (...)
        {
            ret.push_back(0);
        }
    for (unsigned i = s; i < _s.size(); i += 2)
        try
        {
            ret.push_back((uint8_t)(FromHex(_s[i]) * 16 + FromHex(_s[i + 1])));
        }
        catch (...)
        {
            ret.push_back(0);
        }
    return ret;
}

std::string POW::BlockhashToHexString(ethash_h256_t* _hash)
{
    return BytesToHexString((uint8_t*)_hash, 32);
}

int POW::FromHex(char _i)
{
    if (_i >= '0' && _i <= '9')
        return _i - '0';
    if (_i >= 'a' && _i <= 'f')
        return _i - 'a' + 10;
    if (_i >= 'A' && _i <= 'F')
        return _i - 'A' + 10;
    return -1;
}

ethash_h256_t POW::StringToBlockhash(std::string const& _s)
{
    ethash_h256_t ret;
    std::vector<uint8_t> b = HexStringToBytes(_s);
    memcpy(&ret, b.data(), b.size());
    return ret;
}

ethash_h256_t POW::DifficultyLevelInInt(uint8_t difficulty)
{
    uint8_t b[UINT256_SIZE];
    std::fill(b, b + 32, 0xff);
    uint8_t firstNbytesToSet = difficulty / 8;
    uint8_t nBytesBitsToSet = difficulty % 8;

    for (int i = 0; i < firstNbytesToSet; i++)
    {
        b[i] = 0;
    }
    const unsigned char masks[9]
        = {0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x01, 0x00};
    b[firstNbytesToSet] = masks[nBytesBitsToSet];
    return StringToBlockhash(BytesToHexString(b, UINT256_SIZE));
}

ethash_light_t POW::EthashLightNew(uint64_t block_number)
{
    return ethash_light_new(block_number);
}

void POW::EthashLightDelete(ethash_light_t light)
{
    ethash_light_delete(light);
}

bool POW::EthashConfigureLightClient(uint64_t block_number)
{
    std::lock_guard<std::mutex> g(m_mutexLightClientConfigure);
    if (block_number != currentBlockNum)
    {
        EthashLightDelete(ethash_light_client);
        ethash_light_client = EthashLightNew(block_number);
        currentBlockNum = block_number;
    }

    if (block_number < currentBlockNum)
    {
        LOG_GENERAL(INFO,
                    "WARNING: How come the latest block number is smaller than "
                    "I what?");
    }

    return true;
}

ethash_return_value_t POW::EthashLightCompute(ethash_light_t& light,
                                              ethash_h256_t const& header_hash,
                                              uint64_t nonce)
{
    return ethash_light_compute(light, header_hash, nonce);
}

ethash_full_t POW::EthashFullNew(ethash_light_t& light,
                                 ethash_callback_t& CallBack)
{
    return ethash_full_new(light, CallBack);
}

void POW::EthashFullDelete(ethash_full_t& full) { ethash_full_delete(full); }

ethash_return_value_t POW::EthashFullCompute(ethash_full_t& full,
                                             ethash_h256_t const& header_hash,
                                             uint64_t nonce)
{
    return ethash_full_compute(full, header_hash, nonce);
}

ethash_mining_result_t POW::MineLight(ethash_light_t& light,
                                      ethash_h256_t const& header_hash,
                                      ethash_h256_t& difficulty)
{
    uint64_t nonce = std::time(0);
    while (shouldMine)
    {
        ethash_return_value_t mineResult
            = EthashLightCompute(light, header_hash, nonce);
        if (ethash_check_difficulty(&mineResult.result, &difficulty))
        {
            ethash_mining_result_t winning_result
                = {BlockhashToHexString(&mineResult.result),
                   BlockhashToHexString(&mineResult.mix_hash), nonce, true};
            return winning_result;
        }
        nonce++;
    }

    ethash_mining_result_t failure_result = {"", "", 0, false};
    return failure_result;
}

ethash_mining_result_t POW::MineFull(ethash_full_t& full,
                                     ethash_h256_t const& header_hash,
                                     ethash_h256_t& difficulty)
{
    uint64_t nonce = std::time(0);
    while (shouldMine)
    {
        ethash_return_value_t mineResult
            = EthashFullCompute(full, header_hash, nonce);
        if (ethash_check_difficulty(&mineResult.result, &difficulty))
        {
            ethash_mining_result_t winning_result
                = {BlockhashToHexString(&mineResult.result),
                   BlockhashToHexString(&mineResult.mix_hash), nonce, true};
            return winning_result;
        }
        nonce++;
    }

    ethash_mining_result_t failure_result = {"", "", 0, false};
    return failure_result;
}

bool POW::VerifyLight(ethash_light_t& light, ethash_h256_t const& header_hash,
                      uint64_t winning_nonce, ethash_h256_t& difficulty,
                      ethash_h256_t& result, ethash_h256_t& mixhash)
{
    ethash_return_value_t mineResult
        = EthashLightCompute(light, header_hash, winning_nonce);
    if (ethash_check_difficulty(&mineResult.result, &difficulty))
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool POW::VerifyFull(ethash_full_t& full, ethash_h256_t const& header_hash,
                     uint64_t winning_nonce, ethash_h256_t& difficulty,
                     ethash_h256_t& result, ethash_h256_t& mixhash)
{
    ethash_return_value_t mineResult
        = EthashFullCompute(full, header_hash, winning_nonce);
    if (ethash_check_difficulty(&mineResult.result, &difficulty))
    {
        return true;
    }
    else
    {
        return false;
    }
}

std::vector<unsigned char>
POW::ConcatAndhash(const std::array<unsigned char, UINT256_SIZE>& rand1,
                   const std::array<unsigned char, UINT256_SIZE>& rand2,
                   const boost::multiprecision::uint128_t& ipAddr,
                   const PubKey& pubKey)
{
    std::vector<unsigned char> vec;
    for (const auto& s1 : rand1)
    {
        vec.push_back(s1);
    }

    for (const auto& s1 : rand2)
    {
        vec.push_back(s1);
    }

    std::vector<unsigned char> ipAddrVec;
    Serializable::SetNumber<boost::multiprecision::uint128_t>(
        ipAddrVec, 0, ipAddr, UINT128_SIZE);
    vec.insert(std::end(vec), std::begin(ipAddrVec), std::end(ipAddrVec));

    pubKey.Serialize(vec, vec.size());

    SHA2<256> sha2;
    sha2.Update(vec);
    std::vector<unsigned char> sha2_result = sha2.Finalize();
    return sha2_result;
}

ethash_mining_result_t
POW::PoWMine(const boost::multiprecision::uint256_t& blockNum,
             uint8_t difficulty,
             const std::array<unsigned char, UINT256_SIZE>& rand1,
             const std::array<unsigned char, UINT256_SIZE>& rand2,
             const boost::multiprecision::uint128_t& ipAddr,
             const PubKey& pubKey, bool fullDataset)
{
    LOG_MARKER();
    // mutex required to prevent a new mining to begin before previous mining operation has ended(ie. shouldMine=false
    // has been processed) and result.success has been returned)
    std::lock_guard<std::mutex> g(m_mutexPoWMine);
    EthashConfigureLightClient((uint64_t)blockNum);
    ethash_h256_t diffForPoW = DifficultyLevelInInt(difficulty);
    std::vector<unsigned char> sha3_result
        = ConcatAndhash(rand1, rand2, ipAddr, pubKey);

    // Let's hash the inputs before feeding to ethash
    ethash_h256_t headerHash
        = StringToBlockhash(DataConversion::Uint8VecToHexStr(sha3_result));
    ethash_mining_result_t result;

    shouldMine = true;

    if (fullDataset)
    {
        ethash_callback_t CallBack = NULL;
        ethash_full_t fullClient
            = POW::EthashFullNew(ethash_light_client, CallBack);
        result = MineFull(fullClient, headerHash, diffForPoW);
        EthashFullDelete(fullClient);
    }
    else
    {
        result = MineLight(ethash_light_client, headerHash, diffForPoW);
    }
    return result;
}

bool POW::PoWVerify(const boost::multiprecision::uint256_t& blockNum,
                    uint8_t difficulty,
                    const std::array<unsigned char, UINT256_SIZE>& rand1,
                    const std::array<unsigned char, UINT256_SIZE>& rand2,
                    const boost::multiprecision::uint128_t& ipAddr,
                    const PubKey& pubKey, bool fullDataset,
                    uint64_t winning_nonce, std::string& winning_result,
                    std::string& winning_mixhash)
{
    LOG_MARKER();
    EthashConfigureLightClient((uint64_t)blockNum);
    ethash_h256_t diffForPoW = DifficultyLevelInInt(difficulty);
    std::vector<unsigned char> sha3_result
        = ConcatAndhash(rand1, rand2, ipAddr, pubKey);
    ethash_h256_t headerHash
        = StringToBlockhash(DataConversion::Uint8VecToHexStr(sha3_result));
    ethash_h256_t winnning_result = StringToBlockhash(winning_result);
    ethash_h256_t winnning_mixhash = StringToBlockhash(winning_mixhash);

    ethash_h256_t check_hash;
    ethash_quick_hash(&check_hash, &winnning_result, winning_nonce,
                      &winnning_mixhash);
    const std::string check_hash_string
        = BytesToHexString((uint8_t*)&check_hash, POW_SIZE);

    //TODO: check mix hash to prevent DDOS attack
    if (check_hash_string != winning_result)
    {
        //std::cout << check_hash_string << std::endl;
        //std::cout << winning_result << std::endl;
        //return false;
    }

    bool result;
    if (fullDataset)
    {
        ethash_callback_t CallBack = NULL;
        ethash_full_t fullClient
            = POW::EthashFullNew(ethash_light_client, CallBack);
        result = VerifyFull(fullClient, headerHash, winning_nonce, diffForPoW,
                            winnning_result, winnning_mixhash);
        EthashFullDelete(fullClient);
    }
    else
    {
        result = VerifyLight(ethash_light_client, headerHash, winning_nonce,
                             diffForPoW, winnning_result, winnning_mixhash);
    }
    return result;
}
