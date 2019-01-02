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
