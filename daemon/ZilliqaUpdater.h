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

#ifndef ZILLIQA_UPDATER_ZILLIQAUPDATER_H_
#define ZILLIQA_UPDATER_ZILLIQAUPDATER_H_

#include "libUpdater/UpdatePipe.h"

#include <boost/asio/deadline_timer.hpp>

#include <json/json.h>

#include <filesystem>
#include <optional>
#include <thread>

class ZilliqaUpdater final {
 public:
  using GetProcIdByNameFunc =
      std::function<std::vector<pid_t>(const std::string&)>;

  ~ZilliqaUpdater() noexcept;

  template <typename F>
  explicit ZilliqaUpdater(F&& getProcIdByNameFunc)
      : m_getProcByNameFunc{std::forward<F>(getProcIdByNameFunc)} {
    InitLogger();
  }

  void Start();
  void Stop();

  bool Updating() const {
    std::unique_lock<std::mutex> guard{m_mutex};
    return !!m_updateState;
  }
  bool Update();

 private:
  struct UpdateState {
    std::filesystem::path InputPath;
    bool Acknowledged = false;
  };

  std::thread m_updateThread;
  boost::asio::io_context m_ioContext;
  GetProcIdByNameFunc m_getProcByNameFunc;
  mutable std::mutex m_mutex;
  std::unique_ptr<zil::UpdatePipe> m_pipe;
  std::optional<UpdateState> m_updateState;

  void InitLogger();
  void StartUpdateThread();
  void ScheduleUpdateCheck(boost::asio::deadline_timer& updateTimer);
  void CheckUpdate();

  void ExecuteManifest(const Json::Value& manifest);

  void Download(const Json::Value& manifest);
  void Upgrade(const Json::Value& manifest);
  void HandleReply(std::string_view cmd, pid_t zilliqaPid,
                   uint64_t quiesceDSBlock);
};

#endif  // ZILLIQA_UPDATER_ZILLIQAUPDATER_H_
