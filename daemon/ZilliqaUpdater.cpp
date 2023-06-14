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

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/start_dir.hpp>
#include <boost/process/v2/stdio.hpp>

#include <filesystem>
#include <fstream>

#if 0
namespace zil {
class ZilliqaListener {
 public:
  ZilliqaListener(boost::asio::io_context& ioContext, pid_t pid)
      : m_pipe{ioContext, pid} {}

  void Start() { m_pipe.Start(); }
  void Stop() { m_pipe.Stop(); }

 private:
  UpdatePipe m_pipe;
};

}  // namespace zil
#endif

ZilliqaUpdater::~ZilliqaUpdater() noexcept {
  m_ioContext.stop();
  m_updateThread.join();
}

void ZilliqaUpdater::InitLogger() {
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
    Upgrade(manifest);
  } else {
  }
}

void ZilliqaUpdater::Download(const Json::Value& manifest) {
  // TODO: grab the file remotely

  const std::filesystem::path inputFilePath{manifest["input-path"].asString()};

  boost::asio::readable_pipe pipe{m_ioContext};

  // TODO: consider using OpenSSL for this
  boost::process::v2::process checksumProcess{
      m_ioContext,
      "/usr/bin/sha256sum",
      {inputFilePath.c_str()},
      boost::process::v2::process_stdio{{}, pipe, {}}};

  auto exitCode = checksumProcess.wait();
  if (exitCode != 0) {
    throw std::runtime_error{"failed to extract verify the hash of file " +
                             inputFilePath.string() +
                             "(exit code = " + std::to_string(exitCode) + ')'};
  }

  boost::system::error_code errorCode;
  std::string output;
  while (!errorCode) {
    std::array<char, 1024> buffer;
    auto byteCount = pipe.read_some(boost::asio::buffer(buffer), errorCode);
    output += std::string_view{buffer.data(), byteCount};
  }

  if (output.size() > 64) output = output.substr(0, 64);

  std::string sha256;
  boost::to_lower_copy(std::back_inserter(sha256),
                       manifest["sha256"].asString());
  boost::to_lower(output);

  if (output != sha256) {
    throw std::runtime_error{"checksum failed; expected " + sha256 +
                             " but got " + output};
  }
}

void ZilliqaUpdater::Upgrade(const Json::Value& manifest) {
  // TODO:

  const std::filesystem::path inputFilePath{manifest["input-path"].asString()};
  const std::filesystem::path outputPath =
      std::filesystem::temp_directory_path() / "zilliqa-updater";

  boost::process::v2::process untarProcess{
      m_ioContext,
      "/usr/bin/tar",
      {"xfv", inputFilePath.c_str()},
      boost::process::v2::process_start_dir{outputPath.string()}};

  auto exitCode = untarProcess.wait();
  if (exitCode != 0) {
    throw std::runtime_error{"failed to extract downloaded file " +
                             inputFilePath.string() +
                             "(exit code = " + std::to_string(exitCode) + ')'};
  }

  auto pids = m_getProcByNameFunc("zilliqa");
  if (pids.size() != 1) {
    throw std::runtime_error{"unexpected number of zilliqa processes (" +
                             std::to_string(pids.size()) + ')'};
  }

  auto zilliqPid = pids.front();
  try {
    m_updating = true;
    m_pipe = std::make_unique<zil::UpdatePipe>(m_ioContext, zilliqPid);
    m_pipe->OnCommand = [](std::string_view cmd) {
      LOG(INFO) << "Received from zilliqa: " << cmd;
    };
    m_pipe->Start();
    m_pipe->SyncWrite('|' + std::to_string(zilliqPid) + ',' +
                      manifest["quiesce-at-dsblock"].asString() + ',' +
                      manifest["upgrade-at-dsblock"].asString() + '|');
  } catch (...) {
    m_updating = false;
  }
}
