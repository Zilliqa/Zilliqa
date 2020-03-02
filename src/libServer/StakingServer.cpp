/*
 * Copyright (C) 2020 Zilliqa
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
#include "StakingServer.h"
#include "JSONConversion.h"
#include "libUtils/Logger.h"

using namespace jsonrpc;
using namespace std;

StakingServer::StakingServer(Mediator& mediator,
                             jsonrpc::AbstractServerConnector& server)
    : Server(mediator),
      jsonrpc::AbstractServer<StakingServer>(server,
                                             jsonrpc::JSONRPC_SERVER_V2) {
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetRawDSBlock", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &StakingServer::GetRawDSBlockI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetRawTxBlock", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &StakingServer::GetRawTxBlockI);
}

Json::Value StakingServer::GetRawDSBlock(const string& blockNum) {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  try {
    uint64_t BlockNum = stoull(blockNum);
    return JSONConversion::convertRawDSBlocktoJson(
        m_mediator.m_dsBlockChain.GetBlock(BlockNum));
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (runtime_error& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_INVALID_PARAMS, "String not numeric");
  } catch (invalid_argument& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_INVALID_PARAMS, "Invalid arugment");
  } catch (out_of_range& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_INVALID_PARAMS, "Out of range");
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value StakingServer::GetRawTxBlock(const string& blockNum) {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  try {
    uint64_t BlockNum = stoull(blockNum);
    return JSONConversion::convertRawTxBlocktoJson(
        m_mediator.m_txBlockChain.GetBlock(BlockNum));
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (runtime_error& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_INVALID_PARAMS, "String not numeric");
  } catch (invalid_argument& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_INVALID_PARAMS, "Invalid arugment");
  } catch (out_of_range& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_INVALID_PARAMS, "Out of range");
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_MISC_ERROR, "Unable To Process");
  }
}