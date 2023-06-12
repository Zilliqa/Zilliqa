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

  auto manifestPath = std::filesystem::path{"/tmp"} / ".zilliqa-manifest";
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
    return;
  }

  ExecuteManifest(manifest);
} catch (std::exception& e) {
  //
} catch (...) {
  //
}

void ZilliqaUpdater::ExecuteManifest(const Json::Value& manifest) {
  const std::string& version = manifest["version"].asString();
  if (version == VERSION_TAG) return;

  const std::string& action = manifest["action"].asString();
  if (action == "download") {
    Download(manifest);
  } else if (action == "upgrade") {
  } else {
  }
}

void ZilliqaUpdater::Download(const Json::Value& manifest) {
  // TODO: grab the file remotely

  const std::filesystem::path inputFilePath{manifest["input-path"].asString()};

  boost::process::v2::process tarGzProcess{
      m_ioContext, "/usr/bin/tar", {"xfv", inputFilePath.string()}};
  if (tarGzProcess.wait() != 0) return;

#if 0
  std::ifstream inputFile{inputFilePath,
                          std::ios_base::in | std::ios_base::binary};
  boost::iostreams::filtering_streambuf<boost::iostreams::input> inbuf;
  inbuf.push(boost::iostreams::gzip_decompressor());
  inbuf.push(inputFile);

  const std::filesystem::path outputFilePath{
      manifest["output-path"].asString()};
  std::ofstream outputFile{outputFilePath,
                           std::ios_base::out | std::ios_base::binary};

  // Convert streambuf to istream
  std::istream instream(&inbuf);

  outputFile << instream.rdbuf();
#endif
}

void ZilliqaUpdater::Upgrade(const Json::Value& manifest) {
  // TODO:
}
