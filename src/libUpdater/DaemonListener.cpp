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

#include <filesystem>

namespace zil {

void DaemonListener::Start() { m_pipe.Start(); }
void DaemonListener::Stop() { m_pipe.Stop(); }

void DaemonListener::parseCmd(std::string_view cmd) try {
  LOG_GENERAL(DEBUG, "Received command: " << cmd);

  auto first = cmd.find(",");
  if (first == std::string::npos) return;

  auto last = cmd.find(",", first + 1);
  if (last == std::string::npos) return;

  std::size_t pos = 0;
  std::string s{cmd.data(), first};
  pid_t pid = std::stoi(s, &pos);
  if (pid != getpid() || pos != s.size()) {
    LOG_GENERAL(
        WARNING,
        "Ignoring invalid request from daemon meant for a different process");
    return;
  }

  s = std::string{cmd.data() + first + 1, last - first - 1};
  uint64_t quiesceDSBlock = std::stoull(s, &pos);
  if (pos != s.size()) return;

  s = std::string{cmd.data() + last + 1, cmd.size() - last - 1};
  uint64_t updateDSBlock = std::stoull(s, &pos);
  if (pos != s.size()) return;

  auto currentDSBlockNumber = m_lastDSBlockNumberProvider();
  if (currentDSBlockNumber >= quiesceDSBlock ||
      updateDSBlock <= quiesceDSBlock) {
    LOG_GENERAL(WARNING,
                "Ignoring invalid request from daemon to quiesce at block "
                    << quiesceDSBlock << " and update at block "
                    << updateDSBlock);

    m_pipe.AsyncWrite("|" + std::to_string(getpid()) + ",REJECT|");
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

  m_pipe.AsyncWrite("|" + std::to_string(getpid()) + ",OK|");
} catch (std::exception &e) {
  LOG_GENERAL(WARNING,
              "ignoring message from daemon due to exception: " << e.what());
}

}  // namespace zil
