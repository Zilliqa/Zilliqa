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

#include "BaseDB.h"
#include <bsoncxx/stdx/make_unique.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/logger.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>
#include "common/Constants.h"

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

using namespace std;

void BaseDB::Init(unsigned int port) {
  auto instance = bsoncxx::stdx::make_unique<mongocxx::instance>();
  try {
    m_inst = move(instance);
    string uri = "mongodb://" + DB_HOST + ":" + to_string(port);
    mongocxx::uri URI(uri);
    // mongocxx::client client(URI);
    mongocxx::pool pool(URI);
    m_pool = bsoncxx::stdx::make_unique<mongocxx::pool>(URI);
    auto c = m_pool->acquire();
    (*c)[m_dbname].drop();
    // m_client = move(client);
    // m_client[m_dbname].drop();
    m_isInitialized = true;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed to initialized DB " << e.what());
  }
}
