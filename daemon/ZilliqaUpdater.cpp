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

#include "ZilliqaUpdater.h"

#include "libUtils/Logger.h"
#include "libUtils/SWInfo.h"

#if 0
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#endif

#include <boost/process/v2/process.hpp>

#include <filesystem>
#include <fstream>

ZilliqaUpdater::~ZilliqaUpdater() noexcept {
  m_ioContext.stop();
  m_updateThread.join();
}

ZilliqaUpdater::ZilliqaUpdater() {
  INIT_FILE_LOGGER("zilliqad", std::filesystem::current_path())
}

void ZilliqaUpdater::Start() { StartUpdateThread(); }

void ZilliqaUpdater::Stop() {}

void ZilliqaUpdater::StartUpdateThread() {
  m_updateThread = std::thread{[this]() {
    boost::asio::deadline_timer updateTimer{m_ioContext};

    ScheduleUpdateCheck(updateTimer);
    m_ioContext.run();
  }};
}

void ZilliqaUpdater::ScheduleUpdateCheck(
    boost::asio::deadline_timer& updateTimer) {
  updateTimer.expires_from_now(boost::posix_time::seconds{5});
  updateTimer.async_wait(
      [this, &updateTimer](const boost::system::error_code& errorCode) {
        if (errorCode) return;

        CheckUpdate();
        ScheduleUpdateCheck(updateTimer);
      });
}

void ZilliqaUpdater::CheckUpdate() try {
  // TODO: check for file to download from remote URL

  auto manifestPath =
      std::filesystem::temp_directory_path() / "zilliqa-updater" / "manifest";
  std::ifstream manifestFile{manifestPath};
  if (!manifestFile) return;

  std::string content;
  while (manifestFile) {
    std::string line;
    std::getline(manifestFile, line);
    content += line;
  }

  Json::CharReaderBuilder readBuilder;
  auto reader = readBuilder.newCharReader();

  std::string errors;
  Json::Value manifest;
  if (!reader->parse(content.data(), content.data() + content.size(), &manifest,
                     &errors)) {
    // caught below
    throw std::runtime_error{"failed to parse manifest (" + errors + ')'};
  }

  ExecuteManifest(manifest);
} catch (std::exception& e) {
  LOG(WARNING) << "Error while checking for updates: " << e.what();
} catch (...) {
  LOG(WARNING) << "Unexpected error while checking for updates";
}

void ZilliqaUpdater::ExecuteManifest(const Json::Value& manifest) {
  const std::string& version = manifest["version"].asString();
  if (version == VERSION_TAG) return;

  const std::string& action = manifest["action"].asString();
  if (action == "download") {
    Download(manifest);
  } else if (action == "upgrade") {
  } else {
    Upgrade(manifest);
  }
}

void ZilliqaUpdater::Download(const Json::Value& manifest) {
  // TODO: grab the file remotely

  const std::filesystem::path inputFilePath{manifest["input-path"].asString()};
  const std::filesystem::path outputPath =
      std::filesystem::temp_directory_path() / "zilliqa-updater" / "manifest";

  boost::process::v2::process tarGzProcess{
      m_ioContext,
      "/usr/bin/tar",
      {"xfv", inputFilePath.string(), "-C", outputPath.string()}};
  if (tarGzProcess.wait() != 0) {
    throw std::runtime_error{"failed to extract downloaded file " +
                             inputFilePath.string()};
  }
}

void ZilliqaUpdater::Upgrade(const Json::Value& manifest) {
  // TODO:
}
