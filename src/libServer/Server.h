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

#ifndef __SERVER_H__
#define __SERVER_H__

#include <mutex>
#include <random>
#include "jsonrpccpp/server.h"
#include "libData/BlockData/BlockHeader/BlockHeaderBase.h"
#include "libData/DataStructures/CircularArray.h"
#include "libMediator/Mediator.h"

class Mediator;

class Server {
 protected:
  Mediator& m_mediator;
  Server(Mediator& mediator) : m_mediator(mediator) {}
  ~Server();

 public:
  enum RPCErrorCode {
    //! Standard JSON-RPC 2.0 errors
    // RPC_INVALID_REQUEST is internally mapped to HTTP_BAD_REQUEST (400).
    // It should not be used for application-layer errors.
    RPC_INVALID_REQUEST = -32600,
    // RPC_METHOD_NOT_FOUND is internally mapped to HTTP_NOT_FOUND (404).
    // It should not be used for application-layer errors.
    RPC_METHOD_NOT_FOUND = -32601,
    RPC_INVALID_PARAMS = -32602,
    // RPC_INTERNAL_ERROR should only be used for genuine errors in bitcoind
    // (for example datadir corruption).
    RPC_INTERNAL_ERROR = -32603,
    RPC_PARSE_ERROR = -32700,

    //! General application defined errors
    RPC_MISC_ERROR = -1,  //!< std::exception thrown in command handling
    RPC_TYPE_ERROR = -3,  //!< Unexpected type was passed as parameter
    RPC_INVALID_ADDRESS_OR_KEY = -5,  //!< Invalid address or key
    RPC_INVALID_PARAMETER = -8,  //!< Invalid, missing or duplicate parameter
    RPC_DATABASE_ERROR = -20,    //!< Database error
    RPC_DESERIALIZATION_ERROR =
        -22,  //!< Error parsing or validating structure in raw format
    RPC_VERIFY_ERROR =
        -25,  //!< General error during transaction or block submission
    RPC_VERIFY_REJECTED =
        -26,  //!< Transaction or block was rejected by network rules
    RPC_IN_WARMUP = -28,          //!< Client still warming up
    RPC_METHOD_DEPRECATED = -32,  //!< RPC method is deprecated
  };

  inline virtual void GetCurrentMiniEpochI(const Json::Value& request,
                                           Json::Value& response) {
    (void)request;
    response = this->GetCurrentMiniEpoch();
  }

  inline virtual void GetCurrentDSEpochI(const Json::Value& request,
                                         Json::Value& response) {
    (void)request;
    response = this->GetCurrentDSEpoch();
  }

  inline virtual void GetNodeTypeI(const Json::Value& request,
                                   Json::Value& response) {
    (void)request;
    response = this->GetNodeType();
  }

  inline virtual void GetPrevDSDifficultyI(const Json::Value& request,
                                           Json::Value& response) {
    (void)request;
    response = this->GetPrevDSDifficulty();
  }
  inline virtual void GetPrevDifficultyI(const Json::Value& request,
                                         Json::Value& response) {
    (void)request;
    response = this->GetPrevDifficulty();
  }

  virtual std::string GetCurrentMiniEpoch();
  virtual std::string GetCurrentDSEpoch();
  virtual std::string GetNodeType();
  virtual uint8_t GetPrevDSDifficulty();
  virtual uint8_t GetPrevDifficulty();
};

#endif  //__SERVER_H__