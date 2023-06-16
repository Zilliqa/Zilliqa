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

UpdatePipe::~UpdatePipe() noexcept { Stop(); }

void UpdatePipe::Start() { readSome(); }

void UpdatePipe::Stop() {
  m_readPipe.close();
  m_writePipe.close();
}

bool UpdatePipe::SyncWrite(const std::string &buffer) {
  std::size_t bytesWritten = 0;
  while (bytesWritten < buffer.size()) {
    boost::system::error_code errorCode;
    auto count = m_writePipe.write_some(
        boost::asio::const_buffer(buffer.data() + bytesWritten,
                                  buffer.size() - bytesWritten),
        errorCode);
    if (errorCode) {
      LOG_GENERAL(WARNING, "Failed to write to pipe: "
                               << errorCode.what() << " (" << errorCode << ')');

      if (errorCode == boost::asio::error::eof) {
        // createWritePipe();
      } else {
        break;
      }
    }
    bytesWritten += count;
  }

  return bytesWritten == buffer.size();
}

void UpdatePipe::createReadPipe() {
  if (m_readPipe.native_handle() > 0) {
    close(m_readPipe.native_handle());
  }

  m_readPipe = createPipe(m_readBaseName, O_RDWR);
}

void UpdatePipe::createWritePipe() {
  if (m_writePipe.native_handle() > 0) {
    close(m_writePipe.native_handle());
  }

  m_writePipe = createPipe(m_writeBaseName, O_RDWR);
}

boost::asio::posix::stream_descriptor UpdatePipe::createPipe(
    const std::string &baseName, int flag) try {
  const auto &pipeName = std::filesystem::temp_directory_path() /
                         (baseName + '.' + std::to_string(m_pid) + ".pipe");

  // Fail silently since it might already exist due to either zilliqad or
  // zilliqa.
  mkfifo(pipeName.c_str(), 0666);

  auto result = boost::asio::posix::stream_descriptor{m_ioContext};
  auto fd = open(pipeName.c_str(), flag | O_NONBLOCK);
  if (fd <= 0) {
    LOG_GENERAL(WARNING, "Failed to open pipe "
                             << pipeName
                             << "; can't listen to updates from daemon");
  } else {
    LOG_GENERAL(INFO, "Open pipe " << pipeName << " successfully with flags = "
                                   << (flag | O_NONBLOCK));
    result = boost::asio::posix::stream_descriptor{m_ioContext, fd};
  }

  return result;
} catch (std::exception &e) {
  LOG_GENERAL(WARNING, "Exception while creating pipe to daemon: " << e.what());
  return boost::asio::posix::stream_descriptor{m_ioContext};
} catch (...) {
  LOG_GENERAL(WARNING, "Unknown exception while creating pipe to daemon");
  return boost::asio::posix::stream_descriptor{m_ioContext};
}

void UpdatePipe::readSome() {
  m_readPipe.async_read_some(
      boost::asio::buffer(m_readBuffer),
      [this](const boost::system::error_code &errorCode, std::size_t size) {
        if (errorCode) {
          LOG_GENERAL(WARNING, "Error reading from pipe: " << errorCode.what()
                                                           << " (" << errorCode
                                                           << ')');
          if (errorCode == boost::asio::error::eof) {
            // Upon EOF we try to recreate/open the pipe
            m_timer.expires_from_now(boost::posix_time::seconds{5});
            m_timer.async_wait(
                [this](const boost::system::error_code &errorCode) {
                  if (errorCode) return;

                  // createReadPipe();
                  readSome();
                });
          }

          return;
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
