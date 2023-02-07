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

#include "MessengerAccountStoreBase.h"
#include "libData/AccountStore/AccountStore.h"
#include "libMessage/ZilliqaMessage.pb.h"
#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"

#include <algorithm>
#include <map>
#include <random>
#include <unordered_set>

using namespace boost::multiprecision;
using namespace std;
using namespace ZilliqaMessage;

template <class T = ProtoAccountStore>
bool SerializeToArray(const T& protoMessage, zbytes& dst,
                      const unsigned int offset);
bool AccountToProtobuf(const Account& account, ProtoAccount& protoAccount);
bool ProtobufToAccount(const ProtoAccount& protoAccount, Account& account,
                       const Address& addr);

template bool
MessengerAccountStoreBase::SetAccountStore<unordered_map<Address, Account>>(
    zbytes& dst, const unsigned int offset,
    const unordered_map<Address, Account>& addressToAccount);
template bool
MessengerAccountStoreBase::GetAccountStore<unordered_map<Address, Account>>(
    const zbytes& src, const unsigned int offset,
    unordered_map<Address, Account>& addressToAccount);
template bool
MessengerAccountStoreBase::GetAccountStore<unordered_map<Address, Account>>(
    const string& src, const unsigned int offset,
    unordered_map<Address, Account>& addressToAccount);

template <class MAP>
bool MessengerAccountStoreBase::SetAccountStore(zbytes& dst,
                                                const unsigned int offset,
                                                const MAP& addressToAccount) {
  ProtoAccountStore result;

  LOG_GENERAL(INFO, "Accounts to serialize: " << addressToAccount.size());

  for (const auto& entry : addressToAccount) {
    ProtoAccountStore::AddressAccount* protoEntry = result.add_entries();
    protoEntry->set_address(entry.first.data(), entry.first.size);
    ProtoAccount* protoEntryAccount = protoEntry->mutable_account();
    if (!AccountToProtobuf(entry.second, *protoEntryAccount)) {
      LOG_GENERAL(WARNING, "AccountToProtobuf failed");
      return false;
    }
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
bool MessengerAccountStoreBase::GetAccountStore(const zbytes& src,
                                                const unsigned int offset,
                                                MAP& addressToAccount) {
  ProtoAccountStore result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed.");
    return false;
  }

  LOG_GENERAL(INFO, "Accounts deserialized: " << result.entries().size());

  for (const auto& entry : result.entries()) {
    Address address;
    Account account;

    copy(entry.address().begin(),
         entry.address().begin() + min((unsigned int)entry.address().size(),
                                       (unsigned int)address.size),
         address.asArray().begin());
    if (!ProtobufToAccount(entry.account(), account, address)) {
      LOG_GENERAL(WARNING, "ProtobufToAccount failed for account at address "
                               << entry.address());
      return false;
    }

    addressToAccount[address] = account;
  }

  return true;
}

template <class MAP>
bool MessengerAccountStoreBase::GetAccountStore(const string& src,
                                                const unsigned int offset,
                                                MAP& addressToAccount) {
  ProtoAccountStore result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed.");
    return false;
  }

  LOG_GENERAL(INFO, "Accounts deserialized: " << result.entries().size());

  for (const auto& entry : result.entries()) {
    Address address;
    Account account;

    copy(entry.address().begin(),
         entry.address().begin() + min((unsigned int)entry.address().size(),
                                       (unsigned int)address.size),
         address.asArray().begin());
    if (!ProtobufToAccount(entry.account(), account, address)) {
      LOG_GENERAL(WARNING, "ProtobufToAccount failed for account at address "
                               << entry.address());
      return false;
    }

    addressToAccount[address] = account;
  }

  return true;
}
