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

#include "libData/AccountData/AccountStore.h"
#include "libMediator/Mediator.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/Retriever.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"

/// Should be run from a folder named "persistence" consisting of the
/// persistence

using namespace std;
int main() {
  PairOfKey key;  // Dummy to initate mediator
  Peer peer;

  LOG_GENERAL(INFO, "Begin");

  Mediator mediator(key, peer);
  Retriever retriever(mediator);

  LOG_GENERAL(INFO, "Start Retrieving States");

  if (!retriever.RetrieveStates()) {
    LOG_GENERAL(FATAL, "RetrieveStates failed");
    return 0;
  }

  LOG_GENERAL(INFO, "finished RetrieveStates");

  if (!retriever.MigrateContractStates()) {
    LOG_GENERAL(WARNING, "MigrateContractStates failed");
  } else {
    LOG_GENERAL(INFO, "Migrate contract data finished");
  }

  return 0;
}
