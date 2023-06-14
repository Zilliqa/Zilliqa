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

void DaemonListener::start() { readSome(); }

void DaemonListener::stop() {
  m_pipe.cancel();
  m_pipe.close();
}

void DaemonListener::createPipe() try {
  const auto &pipeName = std::filesystem::temp_directory_path() /
                         ("zilliqa." + std::to_string(getpid()) + ".pipe");

  if (mkfifo(pipeName.c_str(), 0666) != 0) {
    LOG_GENERAL(WARNING, "Failed to create pipe "
                             << pipeName << " (it might already exists...)");
  }

  m_fd = open(pipeName.c_str(), O_RDWR | O_NONBLOCK);
  if (m_fd <= 0) {
    LOG_GENERAL(WARNING, "Failed to open pipe "
                             << pipeName
                             << "; can't listen to updates from daemon");
    return;
  }

  m_pipe = boost::asio::posix::stream_descriptor{m_ioContext, m_fd};
} catch (std::exception &e) {
  LOG_GENERAL(WARNING, "Exception while creating pipe to daemon: " << e.what());
} catch (...) {
  LOG_GENERAL(WARNING, "Unknown exception while creating pipe to daemon");
}

void DaemonListener::readSome() {
  m_pipe.async_read_some(
      boost::asio::buffer(m_readBuffer),
      [this](const boost::system::error_code &ec, std::size_t size) {
        if (ec) {
          LOG_GENERAL(WARNING, "Error reading from pipe: " << ec.what() << " ("
                                                           << ec << ')');
          if (ec == boost::asio::error::eof) {
            // Upon EOF we try to recreate/open the pipe
            createPipe();
          } else {
            return;
          }
        } else {
          // TODO: limit size of m_read
          m_read += std::string_view{m_readBuffer.data(), size};
          parseRead();
        }

        readSome();
      });
}

void DaemonListener::parseRead() {
  for (auto first = m_read.find("|"); first != std::string::npos;
       first = m_read.find("|")) {
    auto last = m_read.find("|", first + 1);
    if (last == std::string::npos) return;

    std::string_view cmd{m_read.data() + first + 1, last - first - 1};
    parseCmd(cmd);

    // if there was any before first, or the command couldn't be parsed properly
    // treat it as noise and discard
    m_read.erase(0, last);
  }
}

void DaemonListener::parseCmd(std::string_view cmd) try {
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

} catch (std::exception &e) {
  LOG_GENERAL(WARNING,
              "ignoring message from daemon due to exception: " << e.what());
}

#if 0
void write(const std::string &text) {
  m_pipe.write_some(boost::asio::const_buffer(text.data(), text.size()));

#if 0
        [this](const boost::system::error_code& ec, std::size_t size) {
          if (ec) return;
          auto content = std::string_view{m_readBuffer.data(), size};
          LOG_GENERAL(INFO) << "*** Read from zilliqad pipe: " << content;

          readSome();
        });
#endif
}
#endif

}  // namespace zil
