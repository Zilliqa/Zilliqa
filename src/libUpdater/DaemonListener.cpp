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

#include "DaemonListener.h"

#include "libUtils/Logger.h"

#include <json/json.h>
#include <filesystem>

namespace zil {

void DaemonListener::Start() { m_pipe.Start(); }
void DaemonListener::Stop() { m_pipe.Stop(); }

void DaemonListener::parseCmd(std::string_view cmd) try {
  LOG_GENERAL(DEBUG, "Received command: " << cmd);

  Json::CharReaderBuilder readBuilder;
  auto reader = readBuilder.newCharReader();
  std::string errors;
  Json::Value message;
  if (!reader->parse(cmd.data(), cmd.data() + cmd.size(), &message, &errors)) {
    LOG_GENERAL(WARNING, "Failed to parse reply from zilliqa ("
                             << errors << ")... cancelling");
    return;
  }

  if (message["zilliqa-pid"].asInt() != getpid()) {
    LOG_GENERAL(
        WARNING,
        "Ignoring invalid request from daemon meant for a different process");
    return;
  }

  if (!message.isMember("quiesce-at-dsblock") ||
      !message.isMember("upgrade-at-dsblock")) {
    LOG_GENERAL(WARNING, "Malformed request from daemon");
    return;
  }

  // Conversion errors will result in an exception that will abort the upgrade
  const auto quiesceDSBlock = message["quiesce-at-dsblock"].asUInt64();
  const auto updateDSBlock = message["upgrade-at-dsblock"].asUInt64();

  Json::Value reply;
  reply["zilliqa-pid"] = getpid();

  auto currentDSBlockNumber = m_lastDSBlockNumberProvider();
  if (currentDSBlockNumber >= quiesceDSBlock ||
      updateDSBlock <= quiesceDSBlock) {
    LOG_GENERAL(WARNING,
                "Ignoring invalid request from daemon to quiesce at block "
                    << quiesceDSBlock << " and update at block "
                    << updateDSBlock);

    reply["result"] = "reject";
    m_pipe.AsyncWrite(reply.toStyledString());
    return;
  }

  if (m_quiesceDSBlock) {
    assert(m_updateDSBlock);
    LOG_GENERAL(WARNING, "Already planning to quiesce at block "
                             << m_quiesceDSBlock << " and update at block "
                             << m_updateDSBlock << "; will now override");
  }
  m_quiesceDSBlock = quiesceDSBlock;
  m_updateDSBlock = updateDSBlock;
  LOG_GENERAL(WARNING, "Planning to quiesce at block "
                           << m_quiesceDSBlock << " and update at block "
                           << m_updateDSBlock);

  reply["result"] = "ok";
  m_pipe.AsyncWrite(reply.toStyledString());
} catch (std::exception &e) {
  LOG_GENERAL(WARNING,
              "ignoring message from daemon due to exception: " << e.what());
}

}  // namespace zil
