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

#ifndef ZILLIQA_SRC_LIBUPDATER_DAEMONLISTENER_H_
#define ZILLIQA_SRC_LIBUPDATER_DAEMONLISTENER_H_

#include "UpdatePipe.h"

namespace zil {

/**
 * @brief Listens to updates from zilliqad through a pipe.
 */
class DaemonListener {
 public:
  template <typename F>
  DaemonListener(boost::asio::io_context &ioContext,
                 F &&lastDSBlockNumberProvider)
      : m_pipe{ioContext, getpid()},
        m_lastDSBlockNumberProvider{
            std::forward<F>(lastDSBlockNumberProvider)} {
    assert(m_lastDSBlockNumberProvider);

    m_pipe.OnCommand = [this](std::string_view cmd) { parseCmd(cmd); };
  }

  void Start();
  void Stop();

  std::optional<uint64_t> quiesceDSBlock() const {
    u_int64_t result = m_quiesceDSBlock;
    if (result == 0) return std::nullopt;
    return result;
  }

  std::optional<uint64_t> updateDSBlock() const {
    u_int64_t result = m_updateDSBlock;
    if (result == 0) return std::nullopt;
    return result;
  }

 private:
  using LastDSBlockNumberProvider = std::function<uint64_t()>;

  UpdatePipe m_pipe;
  std::atomic_uint64_t m_quiesceDSBlock{0};
  std::atomic_uint64_t m_updateDSBlock{0};
  LastDSBlockNumberProvider m_lastDSBlockNumberProvider;

  void parseCmd(std::string_view cmd);
};

}  // namespace zil

#endif  // ZILLIQA_SRC_LIBUPDATER_DAEMONLISTENER_H_
