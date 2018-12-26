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

#include "MessengerAccountStoreBase.h"
#include "libData/AccountData/AccountStore.h"
#include "libMessage/ZilliqaMessage.pb.h"
#include "libUtils/Logger.h"

#include <algorithm>
#include <map>
#include <random>
#include <unordered_set>

using namespace boost::multiprecision;
using namespace std;
using namespace ZilliqaMessage;

template <class T = ProtoAccountStore>
bool SerializeToArray(const T& protoMessage, bytes& dst,
                      const unsigned int offset);
void AccountToProtobuf(const Account& account, ProtoAccount& protoAccount);
bool ProtobufToAccount(const ProtoAccount& protoAccount, Account& account);

template bool
MessengerAccountStoreBase::SetAccountStore<unordered_map<Address, Account>>(
    bytes& dst, const unsigned int offset,
    const unordered_map<Address, Account>& addressToAccount);
template bool
MessengerAccountStoreBase::GetAccountStore<unordered_map<Address, Account>>(
    const bytes& src, const unsigned int offset,
    unordered_map<Address, Account>& addressToAccount);

template bool MessengerAccountStoreBase::SetAccountStore<map<Address, Account>>(
    bytes& dst, const unsigned int offset,
    const map<Address, Account>& addressToAccount);
template bool MessengerAccountStoreBase::GetAccountStore<map<Address, Account>>(
    const bytes& src, const unsigned int offset,
    map<Address, Account>& addressToAccount);

template <class MAP>
bool MessengerAccountStoreBase::SetAccountStore(bytes& dst,
                                                const unsigned int offset,
                                                const MAP& addressToAccount) {
  ProtoAccountStore result;

  LOG_GENERAL(INFO, "Debug: Total number of accounts to serialize: "
                        << addressToAccount.size());

  for (const auto& entry : addressToAccount) {
    ProtoAccountStore::AddressAccount* protoEntry = result.add_entries();
    protoEntry->set_address(entry.first.data(), entry.first.size);
    ProtoAccount* protoEntryAccount = protoEntry->mutable_account();
    AccountToProtobuf(entry.second, *protoEntryAccount);
    if (!protoEntryAccount->IsInitialized()) {
      LOG_GENERAL(WARNING, "ProtoAccount initialization failed.");
      return false;
    }
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

template <class MAP>
bool MessengerAccountStoreBase::GetAccountStore(const bytes& src,
                                                const unsigned int offset,
                                                MAP& addressToAccount) {
  ProtoAccountStore result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed.");
    return false;
  }

  LOG_GENERAL(INFO, "Debug: Total number of accounts deserialized: "
                        << result.entries().size());

  for (const auto& entry : result.entries()) {
    Address address;
    Account account;

    copy(entry.address().begin(),
         entry.address().begin() + min((unsigned int)entry.address().size(),
                                       (unsigned int)address.size),
         address.asArray().begin());
    if (!ProtobufToAccount(entry.account(), account)) {
      LOG_GENERAL(WARNING, "ProtobufToAccount failed for account at address "
                               << entry.address());
      return false;
    }

    addressToAccount[address] = account;
  }

  return true;
}