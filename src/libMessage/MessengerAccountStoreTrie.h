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
#ifndef __MESSENGERACCOUNTSTORETRIE_H__
#define __MESSENGERACCOUNTSTORETRIE_H__

#include "common/BaseType.h"
#include "depends/libTrie/TrieDB.h"
#include "libData/AccountData/Account.h"

// This class is only used by AccountStoreTrie template class
// If AccountStoreBase.tpp included Messenger.h, we enter into some circular
// dependency issue Putting the messenger functions below into this new class
// avoids that issue

class MessengerAccountStoreTrie {
 public:
  // ============================================================================
  // Primitives
  // ============================================================================
  template <class DB, class MAP>
  static bool SetAccountStoreTrie(
      bytes& dst, const unsigned int offset,
      const dev::SpecificTrieDB<dev::GenericTrieDB<DB>, Address>& stateTrie,
      const std::shared_ptr<MAP>& addressToAccount);
};

#endif  // __MESSENGERACCOUNTSTORETRIE_H__
