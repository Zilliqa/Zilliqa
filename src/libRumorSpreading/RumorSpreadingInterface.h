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

#ifndef __RUMORSPREADINGINTERFACE_H__
#define __RUMORSPREADINGINTERFACE_H__

#include <functional>
#include <memory>
#include <set>
#include <vector>
#include "Message.h"

namespace RRS {

class RumorSpreadingInterface {
 public:
  // DESTRUCTOR
  virtual ~RumorSpreadingInterface();

  // METHODS
  /**
   *  @brief  Start spreading a new rumor.
   *  @param  rumorId The id corresponding to the rumor.
   *  @return Return true if the rumor was successfully added.
   *
   * Add a new rumor that will be spread to the gossip network. The network is
   * known a-priori and this algorithm does not consider new nodes that join the
   * network after the rumor was added. Disconnected nodes will miss the rumor
   * however ths will not affect the rest of the network. A maximum number of
   * O(F) uniformed nodes is expected, where F is the number of disconnected
   * nodes.
   */
  virtual bool addRumor(int rumorId) = 0;

  /**
   *  @brief  Handle a new message.
   *  @param  message The received message
   *  @param  fromMember  The member id of the sender.
   *  @return Return a pair with the source member id and a vector of PULL
   * messages.
   *
   * Handle a new 'message' from peer 'fromPeer'. Ints are used to identify a
   * member and a rumor in order to abstract away the actual member and rumor
   * types.
   */
  virtual std::pair<int, std::vector<Message>> receivedMessage(
      const Message& message, int fromMember) = 0;

  /**
   *  @brief  Advance to next round.
   *  @return Return a pair with the target member id and a vector of PUSH
   * messages.
   *
   * Advance rumor spreading to the next round for all rumor ids. Returns a pair
   * where the first element is the randomly selected member id and the second
   * element is the vector of PUSH messages that will be sent to the selected
   * member.
   */
  virtual std::pair<std::vector<int>, std::vector<Message>> advanceRound() = 0;
};

}  // namespace RRS

#endif  //__RUMORSPREADINGINTERFACE_H__
