/*
 * Copyright (C) 2022 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBSERVER_UNIXDOMAINSOCKETCLIENT_H_
#define ZILLIQA_SRC_LIBSERVER_UNIXDOMAINSOCKETCLIENT_H_

#include <jsonrpccpp/client.h>

// This is a Custom socket handler using asio
// for the client connection to evm-ds and scilla server

namespace rpc {

class UnixDomainSocketClient : public jsonrpc::IClientConnector {
 public:
  explicit UnixDomainSocketClient(const std::string& path) : m_path(path){};

  void SendRPCMessage(const std::string& message, std::string& result) override;

 private:
  std::string m_path;
};

}  // namespace rpc

#endif  // ZILLIQA_SRC_LIBSERVER_UNIXDOMAINSOCKETCLIENT_H_
