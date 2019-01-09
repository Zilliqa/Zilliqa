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

#include "PeerStore.h"

using namespace std;

PeerStore::PeerStore() {}

PeerStore::~PeerStore() {}

PeerStore& PeerStore::GetStore() {
  static PeerStore ps;
  return ps;
}

void PeerStore::AddPeerPair(const PubKey& key, const Peer& peer) {
  lock_guard<mutex> g(m_mutexStore);
  m_store[key] = peer;
}

unsigned int PeerStore::GetPeerCount() const { return m_store.size(); }

Peer PeerStore::GetPeer(const PubKey& key) {
  lock_guard<mutex> g(m_mutexStore);
  try {
    return m_store.at(key);
  } catch (out_of_range& e) {
    return Peer(0, 0);
  }
}

vector<pair<PubKey, Peer>> PeerStore::GetAllPeerPairs() const {
  vector<pair<PubKey, Peer>> result;

  lock_guard<mutex> g(m_mutexStore);

  for (auto const& i : m_store) {
    result.push_back(i);
  }

  return result;
}

vector<Peer> PeerStore::GetAllPeers() const {
  vector<Peer> result;

  lock_guard<mutex> g(m_mutexStore);
  for (const auto& it : m_store) {
    result.emplace_back(it.second);
  }

  return result;
}

vector<PubKey> PeerStore::GetAllKeys() const {
  vector<PubKey> result;

  lock_guard<mutex> g(m_mutexStore);
  for (const auto& it : m_store) {
    result.emplace_back(it.first);
  }

  return result;
}

void PeerStore::RemovePeer(const PubKey& key) {
  lock_guard<mutex> g(m_mutexStore);
  m_store.erase(key);
}

void PeerStore::RemoveAllPeers() {
  lock_guard<mutex> g(m_mutexStore);
  m_store.clear();
}
