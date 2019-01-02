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

#include <boost/algorithm/string/predicate.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

#include "common/Serializable.h"
#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"
#include "pow.h"

#ifdef OPENCL_MINE
#include "depends/libethash-cl/CLMiner.h"
#endif

#ifdef CUDA_MINE
#include "depends/libethash-cuda/CUDAMiner.h"
#endif

POW::POW() {
  m_currentBlockNum = 0;
  m_epochContextLight =
      ethash::create_epoch_context(ethash::get_epoch_number(m_currentBlockNum));

  if (FULL_DATASET_MINE && !CUDA_GPU_MINE && !OPENCL_GPU_MINE) {
    m_epochContextFull = ethash::create_epoch_context_full(
        ethash::get_epoch_number(m_currentBlockNum));
  }

  if (!LOOKUP_NODE_MODE) {
    if (OPENCL_GPU_MINE) {
      InitOpenCL();
    } else if (CUDA_GPU_MINE) {
      InitCUDA();
    }
  }
}

POW::~POW() {}

POW& POW::GetInstance() {
  static POW pow;
  return pow;
}

void POW::StopMining() { m_shouldMine = false; }

std::string POW::BytesToHexString(const uint8_t* str, const uint64_t s) {
  std::ostringstream ret;

  for (size_t i = 0; i < s; ++i)
    ret << std::hex << std::setfill('0') << std::setw(2) << std::nouppercase
        << (int)str[i];

  return ret.str();
}

std::vector<uint8_t> POW::HexStringToBytes(std::string const& _s) {
  unsigned s = (_s[0] == '0' && _s[1] == 'x') ? 2 : 0;
  std::vector<uint8_t> ret;
  ret.reserve((_s.size() - s + 1) / 2);

  if (_s.size() % 2) try {
      ret.push_back(FromHex(_s[s++]));
    } catch (...) {
      ret.push_back(0);
    }
  for (unsigned i = s; i < _s.size(); i += 2) try {
      ret.push_back((uint8_t)(FromHex(_s[i]) * 16 + FromHex(_s[i + 1])));
    } catch (...) {
      ret.push_back(0);
    }
  return ret;
}

std::string POW::BlockhashToHexString(const ethash_hash256& _hash) {
  return BytesToHexString(_hash.bytes, 32);
}

int POW::FromHex(char _i) {
  if (_i >= '0' && _i <= '9') return _i - '0';
  if (_i >= 'a' && _i <= 'f') return _i - 'a' + 10;
  if (_i >= 'A' && _i <= 'F') return _i - 'A' + 10;
  return -1;
}

ethash_hash256 POW::StringToBlockhash(std::string const& _s) {
  ethash_hash256 ret;
  std::vector<uint8_t> b = HexStringToBytes(_s);
  if (b.size() != 32) {
    LOG_GENERAL(WARNING,
                "Input to StringToBlockhash is not of size 32. Returning "
                "uninitialize ethash_hash256. Size is "
                    << b.size());
    return ret;
  }
  copy(b.begin(), b.end(), ret.bytes);
  return ret;
}

bool POW::CheckDificulty(const ethash_hash256& result,
                         const ethash_hash256& boundary) {
  return ethash::is_less_or_equal(result, boundary);
}

ethash_hash256 POW::DifficultyLevelInInt(uint8_t difficulty) {
  uint8_t b[UINT256_SIZE];
  std::fill(b, b + UINT256_SIZE, 0xFF);
  uint8_t firstNbytesToSet = difficulty / 8;
  uint8_t nBytesBitsToSet = difficulty % 8;

  for (int i = 0; i < firstNbytesToSet; i++) {
    b[i] = 0;
  }

  const unsigned char masks[] = {0xFF, 0x7F, 0x3F, 0x1F,
                                 0x0F, 0x07, 0x03, 0x01};
  b[firstNbytesToSet] = masks[nBytesBitsToSet];
  return StringToBlockhash(BytesToHexString(b, UINT256_SIZE));
}

bool POW::EthashConfigureClient(uint64_t block_number, bool fullDataset) {
  std::lock_guard<std::mutex> g(m_mutexLightClientConfigure);

  if (block_number < m_currentBlockNum) {
    LOG_GENERAL(WARNING,
                "WARNING: How come the latest block number is smaller than "
                "current block number? block_number: "
                    << block_number
                    << " currentBlockNum: " << m_currentBlockNum);
  }

  if (ethash::get_epoch_number(block_number) !=
      ethash::get_epoch_number(m_currentBlockNum)) {
    auto epochNumber = ethash::get_epoch_number(block_number);
    m_epochContextLight = ethash::create_epoch_context(epochNumber);
  }

  bool isMineFullCpu = fullDataset && !CUDA_GPU_MINE && !OPENCL_GPU_MINE;

  if (isMineFullCpu && (m_epochContextFull == nullptr ||
                        ethash::get_epoch_number(block_number) !=
                            ethash::get_epoch_number(m_currentBlockNum))) {
    m_epochContextFull = ethash::create_epoch_context_full(
        ethash::get_epoch_number(block_number));
  }

  m_currentBlockNum = block_number;

  return true;
}

ethash_mining_result_t POW::MineLight(ethash_hash256 const& header_hash,
                                      ethash_hash256 const& boundary,
                                      uint64_t startNonce) {
  uint64_t nonce = startNonce;
  while (m_shouldMine) {
    auto mineResult = ethash::hash(*m_epochContextLight, header_hash, nonce);
    if (ethash::is_less_or_equal(mineResult.final_hash, boundary)) {
      ethash_mining_result_t winning_result = {
          BlockhashToHexString(mineResult.final_hash),
          BlockhashToHexString(mineResult.mix_hash), nonce, true};
      return winning_result;
    }
    nonce++;
  }

  ethash_mining_result_t failure_result = {"", "", 0, false};
  return failure_result;
}

ethash_mining_result_t POW::MineFull(ethash_hash256 const& header_hash,
                                     ethash_hash256 const& boundary,
                                     uint64_t startNonce) {
  uint64_t nonce = startNonce;
  while (m_shouldMine) {
    auto mineResult = ethash::hash(*m_epochContextFull, header_hash, nonce);
    if (ethash::is_less_or_equal(mineResult.final_hash, boundary)) {
      ethash_mining_result_t winning_result = {
          BlockhashToHexString(mineResult.final_hash),
          BlockhashToHexString(mineResult.mix_hash), nonce, true};
      return winning_result;
    }
    nonce++;
  }

  ethash_mining_result_t failure_result = {"", "", 0, false};
  return failure_result;
}

ethash_mining_result_t POW::MineFullGPU(uint64_t blockNum,
                                        ethash_hash256 const& header_hash,
                                        uint8_t difficulty,
                                        uint64_t startNonce) {
  std::vector<std::unique_ptr<std::thread>> vecThread;
  uint64_t nonce = startNonce;
  m_minerIndex = 0;
  // Clear old result
  for (auto& miningResult : m_vecMiningResult) {
    miningResult = ethash_mining_result_t{"", "", 0, false};
  }
  for (size_t i = 0; i < m_miners.size(); ++i) {
    vecThread.push_back(std::make_unique<std::thread>(
        [&] { MineFullGPUThread(blockNum, header_hash, difficulty, nonce); }));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  std::unique_lock<std::mutex> lk(m_mutexMiningResult);
  m_cvMiningResult.wait(lk);
  m_shouldMine = false;
  for (auto& ptrThead : vecThread) {
    ptrThead->join();
  }

  for (const auto& miningResult : m_vecMiningResult) {
    if (miningResult.success) {
      return miningResult;
    }
  }

  return ethash_mining_result_t{"", "", 0, false};
}

void POW::MineFullGPUThread(uint64_t blockNum,
                            ethash_hash256 const& header_hash,
                            uint8_t difficulty, uint64_t nonce) {
  LOG_MARKER();
  auto index = m_minerIndex.load(std::memory_order_relaxed);
  ++m_minerIndex;
  LOG_GENERAL(INFO, "Difficulty : " << std::to_string(difficulty)
                                    << ", miner index " << index);
  dev::eth::WorkPackage wp;
  wp.blockNumber = blockNum;
  wp.boundary = (dev::h256)(dev::u256)((dev::bigint(1) << 256) /
                                       (dev::u256(1) << difficulty));

  wp.header = dev::h256{header_hash.bytes, dev::h256::ConstructFromPointer};

  constexpr uint32_t NONCE_SEGMENT_WIDTH = 40;
  const uint64_t NONCE_SEGMENT = (uint64_t)pow(2, NONCE_SEGMENT_WIDTH);
  wp.startNonce = nonce + index * NONCE_SEGMENT;

  dev::eth::Solution solution;
  while (m_shouldMine) {
    if (!m_miners[index]->mine(wp, solution)) {
      LOG_GENERAL(WARNING, "GPU failed to do mine, GPU miner log: "
                               << m_miners[index]->getLog());
      m_vecMiningResult[index] = ethash_mining_result_t{"", "", 0, false};
      m_cvMiningResult.notify_one();
      return;
    }
    auto hashResult = LightHash(blockNum, header_hash, solution.nonce);
    auto boundary = DifficultyLevelInInt(difficulty);
    if (ethash::is_less_or_equal(hashResult.final_hash, boundary)) {
      m_vecMiningResult[index] =
          ethash_mining_result_t{BlockhashToHexString(hashResult.final_hash),
                                 solution.mixHash.hex(), solution.nonce, true};
      m_cvMiningResult.notify_one();
      return;
    }
    wp.startNonce = solution.nonce;
  }
  m_vecMiningResult[index] = ethash_mining_result_t{"", "", 0, false};
  m_cvMiningResult.notify_one();
  return;
}

bytes POW::ConcatAndhash(const std::array<unsigned char, UINT256_SIZE>& rand1,
                         const std::array<unsigned char, UINT256_SIZE>& rand2,
                         const boost::multiprecision::uint128_t& ipAddr,
                         const PubKey& pubKey, uint32_t lookupId,
                         const boost::multiprecision::uint128_t& gasPrice) {
  bytes vec;
  for (const auto& s1 : rand1) {
    vec.push_back(s1);
  }

  for (const auto& s1 : rand2) {
    vec.push_back(s1);
  }

  bytes ipAddrVec;
  Serializable::SetNumber<boost::multiprecision::uint128_t>(
      ipAddrVec, 0, ipAddr, UINT128_SIZE);
  vec.insert(std::end(vec), std::begin(ipAddrVec), std::end(ipAddrVec));

  pubKey.Serialize(vec, vec.size());

  Serializable::SetNumber<uint32_t>(vec, vec.size(), lookupId,
                                    sizeof(uint32_t));
  Serializable::SetNumber<boost::multiprecision::uint128_t>(
      vec, vec.size(), gasPrice, UINT128_SIZE);

  SHA2<256> sha2;
  sha2.Update(vec);
  bytes sha2_result = sha2.Finalize();
  return sha2_result;
}

ethash_hash256 POW::GenHeaderHash(
    const std::array<unsigned char, UINT256_SIZE>& rand1,
    const std::array<unsigned char, UINT256_SIZE>& rand2,
    const boost::multiprecision::uint128_t& ipAddr, const PubKey& pubKey,
    uint32_t lookupId, const boost::multiprecision::uint128_t& gasPrice) {
  bytes sha2_result =
      ConcatAndhash(rand1, rand2, ipAddr, pubKey, lookupId, gasPrice);

  // Let's hash the inputs before feeding to ethash
  return StringToBlockhash(DataConversion::Uint8VecToHexStr(sha2_result));
}

ethash_mining_result_t POW::PoWMine(uint64_t blockNum, uint8_t difficulty,
                                    const ethash_hash256& headerHash,
                                    bool fullDataset, uint64_t startNonce) {
  LOG_MARKER();
  // mutex required to prevent a new mining to begin before previous mining
  // operation has ended(ie. m_shouldMine=false has been processed) and
  // result.success has been returned)
  std::lock_guard<std::mutex> g(m_mutexPoWMine);
  EthashConfigureClient(blockNum, fullDataset);
  auto boundary = DifficultyLevelInInt(difficulty);

  ethash_mining_result_t result;

  m_shouldMine = true;

  if (OPENCL_GPU_MINE || CUDA_GPU_MINE) {
    result = MineFullGPU(blockNum, headerHash, difficulty, startNonce);
  } else if (fullDataset) {
    result = MineFull(headerHash, boundary, startNonce);
  } else {
    result = MineLight(headerHash, boundary, startNonce);
  }
  return result;
}

bool POW::PoWVerify(uint64_t blockNum, uint8_t difficulty,
                    const ethash_hash256& headerHash, uint64_t winning_nonce,
                    const std::string& winning_result,
                    const std::string& winning_mixhash) {
  LOG_MARKER();
  EthashConfigureClient(blockNum);
  const auto boundary = DifficultyLevelInInt(difficulty);
  auto winnning_result = StringToBlockhash(winning_result);
  auto winningMixhash = StringToBlockhash(winning_mixhash);

  if (!ethash::is_less_or_equal(winnning_result, boundary)) {
    LOG_GENERAL(WARNING, "PoW solution doesn't meet difficulty requirement");
    return false;
  }

  return ethash::verify(*m_epochContextLight, headerHash, winningMixhash,
                        winning_nonce, boundary);
}

ethash::result POW::LightHash(uint64_t blockNum,
                              ethash_hash256 const& header_hash,
                              uint64_t nonce) {
  EthashConfigureClient(blockNum);
  return ethash::hash(*m_epochContextLight, header_hash, nonce);
}

bool POW::CheckSolnAgainstsTargetedDifficulty(const ethash_hash256& result,
                                              uint8_t difficulty) {
  const auto boundary = DifficultyLevelInInt(difficulty);
  return ethash::is_less_or_equal(result, boundary);
}

bool POW::CheckSolnAgainstsTargetedDifficulty(const std::string& result,
                                              uint8_t difficulty) {
  const auto boundary = DifficultyLevelInInt(difficulty);
  ethash_hash256 hashResult = StringToBlockhash(result);
  return ethash::is_less_or_equal(hashResult, boundary);
}

void POW::InitOpenCL() {
#ifdef OPENCL_MINE
  using namespace dev::eth;

  CLMiner::setCLKernel(CLKernelName::Stable);

  if (!CLMiner::configureGPU(OPENCL_LOCAL_WORK_SIZE,
                             OPENCL_GLOBAL_WORK_SIZE_MULTIPLIER, 0,
                             OPENCL_START_EPOCH, 0, 0, false, false)) {
    LOG_GENERAL(FATAL, "Failed to configure OpenCL GPU, please check hardware");
  }

  auto gpuToUse = GetGpuToUse();
  auto totalGpuDevice = CLMiner::getNumDevices();

  CLMiner::setNumInstances(gpuToUse.size());

  for (const auto gpuIndex : gpuToUse) {
    if (gpuIndex >= totalGpuDevice) {
      LOG_GENERAL(FATAL, "Selected GPU "
                             << gpuIndex
                             << " exceed the physical OpenCL GPU number "
                             << totalGpuDevice);
    }

    m_miners.push_back(std::make_unique<CLMiner>(gpuIndex));
    m_vecMiningResult.push_back(ethash_mining_result_t{"", "", 0, false});
  }
  LOG_GENERAL(INFO, "OpenCL GPU initialized in POW");
#else
  LOG_GENERAL(FATAL,
              "The software is not build with OpenCL. Please enable the "
              "OpenCL build option and "
              "and build software again");
#endif
}

void POW::InitCUDA() {
#ifdef CUDA_MINE
  using namespace dev::eth;

  auto gpuToUse = GetGpuToUse();
  auto deviceGenerateDag = *gpuToUse.begin();
  LOG_GENERAL(INFO, "Generate dag Nvidia GPU #" << deviceGenerateDag);

  if (!CUDAMiner::configureGPU(CUDA_BLOCK_SIZE, CUDA_GRID_SIZE, CUDA_STREAM_NUM,
                               CUDA_SCHEDULE_FLAG, 0, deviceGenerateDag, false,
                               false)) {
    LOG_GENERAL(FATAL, "Failed to configure CUDA GPU, please check hardware");
  }

  CUDAMiner::setNumInstances(gpuToUse.size());

  auto totalGpuDevice = CUDAMiner::getNumDevices();
  for (const auto gpuIndex : gpuToUse) {
    if (gpuIndex >= totalGpuDevice) {
      LOG_GENERAL(FATAL, "Selected GPU "
                             << gpuIndex
                             << " exceed the physical Nvidia GPU number "
                             << totalGpuDevice);
    }

    m_miners.push_back(std::make_unique<CUDAMiner>(gpuIndex));
    m_vecMiningResult.push_back(ethash_mining_result_t{"", "", 0, false});
  }
  LOG_GENERAL(INFO, "CUDA GPU initialized in POW");
#else
  LOG_GENERAL(FATAL,
              "The software is not build with CUDA. Please enable the CUDA "
              "build option "
              "and build software again");
#endif
}

std::set<unsigned int> POW::GetGpuToUse() {
  std::set<unsigned int> gpuToUse;
  std::stringstream ss(GPU_TO_USE);
  std::string item;
  while (std::getline(ss, item, ',')) {
    unsigned int index = strtol(item.c_str(), NULL, 10);
    gpuToUse.insert(index);
  }

  if (gpuToUse.empty()) {
    LOG_GENERAL(FATAL, "Please select at least one GPU to use.");
  }

  return gpuToUse;
}
