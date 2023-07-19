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

#ifndef ZILLIQA_SRC_LIBUPDATER_UPDATEPIPE_H_
#define ZILLIQA_SRC_LIBUPDATER_UPDATEPIPE_H_

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

namespace zil {

/**
 * @brief Simple representation of the updater pipe that
 *        allows reading & writing asynchronously.
 */
class UpdatePipe final {
 public:
  using OnCommandCallback = std::function<void(std::string_view)>;

  ~UpdatePipe() noexcept;

  template <typename ReadNameT, typename WriteNameT>
  UpdatePipe(boost::asio::io_context &ioContext, pid_t pid,
             ReadNameT &&readBaseName, WriteNameT &writeBaseName)
      : m_ioContext{ioContext},
        m_pid{pid},
        m_timer{m_ioContext},
        m_readBaseName{std::forward<ReadNameT>(readBaseName)},
        m_readPipe{m_ioContext},
        m_writeBaseName{std::forward<WriteNameT>(writeBaseName)},
        m_writePipe{m_ioContext} {
    createReadPipe();
    createWritePipe();
  }

  void Start();
  void Stop();
  void AsyncWrite(const std::string &buffer);

  OnCommandCallback OnCommand;

 private:
  static const constexpr std::size_t READ_SIZE_BUFFER_BYTES = 1024;

  boost::asio::io_context &m_ioContext;
  pid_t m_pid;
  boost::asio::deadline_timer m_timer;
  std::string m_readBaseName;
  boost::asio::posix::stream_descriptor m_readPipe;
  std::string m_writeBaseName;
  boost::asio::posix::stream_descriptor m_writePipe;
  std::array<char, READ_SIZE_BUFFER_BYTES> m_readBuffer;
  std::string m_read;

  boost::asio::posix::stream_descriptor createPipe(const std::string &baseName,
                                                   int flag);
  void createReadPipe();
  void createWritePipe();
  void readSome();
  void parseRead();
};

}  // namespace zil

#endif  // ZILLIQA_SRC_LIBUPDATER_UPDATEPIPE_H_
