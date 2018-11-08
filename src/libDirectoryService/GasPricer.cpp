/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#include "DirectoryService.h"
#include "libMediator/Mediator.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

void DirectoryService::CalculateGasPrice() {
  uint64_t loBlockNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetEpochNum();
  uint64_t hiBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  uint64_t totalBlockNum = hiBlockNum - loBlockNum + 1;
  uint64_t fullBlockNum = 0;

  for (uint64_t i = loBlockNum; i <= hiBlockNum; ++i) {
    uint256_t gasUsed =
        m_mediator.m_txBlockChain.GetBlock(i).GetHeader().GetGasUsed();
    uint256_t gasLimit =
        m_mediator.m_txBlockChain.GetBlock(i).GetHeader().GetGasLimit();
    if (gasUsed >= gasLimit * GAS_CONGESTION_RATE / 100) {
      fullBlockNum++;
    }
  }

  if (fullBlockNum < totalBlockNum * UNFILLED_RATIO_LOW / 100) {
    DecreaseGasPrice();
  } else if (fullBlockNum > totalBlockNum * UNFILLED_RATIO_HIGH / 100) {
    IncreaseGasPrice();
  } else {
    // remain unchanged
  }
}

void DirectoryService::IncreaseGasPrice() {
  multiset<uint256_t> gasProposals;
  for (const auto& soln : m_allDSPoWs) {
    gasProposals.emplace(soln.second.gasprice);
  }
}

void DirectoryService::DecreaseGasPrice() {}