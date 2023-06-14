/*
 * Copyright (C) 2023 Zilliqa
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

#include "UpdatePipe.h"

#include "libUtils/Logger.h"

#include <filesystem>

namespace zil {

void UpdatePipe::Start() { readSome(); }

void UpdatePipe::Stop() {
  m_pipe.cancel();
  m_pipe.close();
}

bool UpdatePipe::SyncWrite(const std::string &buffer) {
  std::size_t bytesWritten = 0;
  while (bytesWritten < buffer.size()) {
    boost::system::error_code ec;
    auto count = m_pipe.write_some(
        boost::asio::const_buffer(buffer.data() + bytesWritten,
                                  buffer.size() - bytesWritten),
        ec);
    if (ec) {
      if (ec == boost::asio::error::eof) {
        createPipe();
      } else {
        break;
      }
    }
    bytesWritten += count;
  }

  return bytesWritten == buffer.size();
}

void UpdatePipe::createPipe() try {
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

void UpdatePipe::readSome() {
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

void UpdatePipe::parseRead() {
  for (auto first = m_read.find("|"); first != std::string::npos;
       first = m_read.find("|")) {
    auto last = m_read.find("|", first + 1);
    if (last == std::string::npos) return;

    std::string_view cmd{m_read.data() + first + 1, last - first - 1};
    if (OnCommand) OnCommand(cmd);

    // if there was any before first, or the command couldn't be parsed properly
    // treat it as noise and discard
    m_read.erase(0, last);
  }
}

}  // namespace zil
