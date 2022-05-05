//
// Created by steve on 04/05/22.
//


#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/iterator_range.hpp>

#include "libUtils/DetachedFunction.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/ScillaUtils.h"
#include "libUtils/SysCommand.h"

#include "EvmClient.h"


/* EvmClient Init */
void EvmClient::Init() {
  LOG_MARKER();
  std::shared_ptr<jsonrpc::UnixDomainSocketClient> conn =
      std::make_shared<jsonrpc::UnixDomainSocketClient>(EVM_SERVER_SOCKET_PATH );
  m_connector = std::make_shared<jsonrpc::UnixDomainSocketClient>(EVM_SERVER_SOCKET_PATH );
  m_client= std::make_shared<jsonrpc::Client>(*m_connector, jsonrpc::JSONRPC_CLIENT_V2);
}

bool EvmClient::CallRunner(uint32_t version, const Json::Value& _json,
                              std::string& result,
                           __attribute__((unused)) uint32_t counter) {

  assert(version == 0);

  try {
    std::lock_guard<std::mutex> g(m_mutexMain);
    result = m_client->CallMethod("run", _json).asString();
  } catch (jsonrpc::JsonRpcException& e) {
    LOG_GENERAL(WARNING, "CallRunner failed: " << e.what());
    result = e.what();
    return false;
  }

  return true;
}

