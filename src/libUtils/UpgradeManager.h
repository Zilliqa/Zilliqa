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

#ifndef __UPGRADEMANAGER_H__
#define __UPGRADEMANAGER_H__

#include <curl/curl.h>
#include <cstring>
#include <memory>
#include <string>
#include "libMediator/Mediator.h"
#include "libUtils/SWInfo.h"

class UpgradeManager {
 private:
  CURL* m_curl;

  UpgradeManager();
  ~UpgradeManager();

  // Singleton should not implement these
  UpgradeManager(UpgradeManager const&) = delete;
  void operator=(UpgradeManager const&) = delete;

 public:
  /// Returns the singleton UpgradeManager instance.
  static UpgradeManager& GetInstance();

  /// Should be only called internally, put in public just for testing
  std::string DownloadFile(const char* fileTail,
                           const char* releaseUrl = nullptr);

  bool LoadInitialDS(std::vector<PubKey>& initialDSCommittee);
};

#endif  // __UPGRADEMANAGER_H__
