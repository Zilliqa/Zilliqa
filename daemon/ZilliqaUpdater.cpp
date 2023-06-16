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

ZilliqaUpdater::~ZilliqaUpdater() noexcept { Stop(); }

void ZilliqaUpdater::InitLogger() {
  INIT_FILE_LOGGER("zilliqad", std::filesystem::current_path())
}

void ZilliqaUpdater::Start() { StartUpdateThread(); }

void ZilliqaUpdater::Stop() {
  m_ioContext.stop();
  m_updateThread.join();
}

void ZilliqaUpdater::StartUpdateThread() {
  m_updateThread = std::thread{[this]() {
    boost::asio::deadline_timer updateTimer{m_ioContext};

    ScheduleUpdateCheck(updateTimer);
    m_ioContext.run();
  }};
}

void ZilliqaUpdater::ScheduleUpdateCheck(
    boost::asio::deadline_timer& updateTimer) {
  updateTimer.expires_from_now(boost::posix_time::seconds{15});
  updateTimer.async_wait(
      [this, &updateTimer](const boost::system::error_code& errorCode) {
        if (errorCode) return;

        if (!Updating()) CheckUpdate();

        ScheduleUpdateCheck(updateTimer);
      });
}

void ZilliqaUpdater::CheckUpdate() try {
  // TODO: check for file to download from remote URL
  const auto updatesDir =
      std::filesystem::temp_directory_path() / "zilliqa" / "updates";
  const auto manifestPath = updatesDir / "manifest";

  const std::string& manifestUrl = "s3://zilliqa/updates/manifest";
  boost::process::v2::process downloadProcess{
      m_ioContext,
      "/usr/local/bin/aws",
      {"s3", "cp", manifestUrl, manifestPath.string()}};

  auto exitCode = downloadProcess.wait();
  if (exitCode != 0) {
    throw std::runtime_error{"failed to download manifest from " + manifestUrl +
                             "(exit code = " + std::to_string(exitCode) + ')'};
  }

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
  LOG_GENERAL(WARNING, "Error while checking for updates: " << e.what());
} catch (...) {
  LOG_GENERAL(WARNING, "Unexpected error while checking for updates");
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
    // ignore
  }
}

void ZilliqaUpdater::Download(const Json::Value& manifest) {
  // TODO: grab the file remotely
  if (!manifest.isMember("uuid") || !manifest.isMember("url") ||
      !manifest.isMember("sha256")) {
    LOG_GENERAL(WARNING, "Malformed download manifest");
    return;
  }

  const auto updateDir = std::filesystem::temp_directory_path() / "zilliqa" /
                         "updates" / manifest["uuid"].asString();

  LOG_GENERAL(INFO, "Creating directory " << updateDir);

  std::error_code errorCode;
  std::filesystem::create_directory(updateDir, errorCode);

  auto outputFilePath = updateDir / "zilliqa.tar.gz";
  const auto& downloadUrl = manifest["url"].asString();
  LOG_GENERAL(INFO, "Downloading from " << downloadUrl);
  boost::process::v2::process downloadProcess{
      m_ioContext,
      "/usr/local/bin/aws",
      {"s3", "cp", downloadUrl, outputFilePath.string()}};

  auto exitCode = downloadProcess.wait();
  if (exitCode != 0) {
    throw std::runtime_error{"failed to download file from " + downloadUrl +
                             "(exit code = " + std::to_string(exitCode) + ')'};
  }

  boost::asio::readable_pipe pipe{m_ioContext};

  // TODO: consider using OpenSSL for this
  boost::process::v2::process checksumProcess{
      m_ioContext,
      "/usr/bin/sha256sum",
      {outputFilePath.string()},
      boost::process::v2::process_stdio{{}, pipe, {}}};

  exitCode = checksumProcess.wait();
  if (exitCode != 0) {
    throw std::runtime_error{"failed to extract verify the hash of file " +
                             outputFilePath.string() +
                             "(exit code = " + std::to_string(exitCode) + ')'};
  }

  // Read output from the pipe and make sure it's a hash
  // that is identical to what we expect
  std::string output;
  {
    boost::system::error_code errorCode;
    while (!errorCode) {
      std::array<char, 1024> buffer;
      auto byteCount = pipe.read_some(boost::asio::buffer(buffer), errorCode);
      output += std::string_view{buffer.data(), byteCount};
    }
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
  if (!manifest.isMember("quiesce-at-dsblock") ||
      !manifest.isMember("upgrade-at-dsblock") || !manifest.isMember("uuid")) {
    LOG_GENERAL(WARNING, "Malformed upgrade manifest");
    return;
  }

  const auto updateDir = std::filesystem::temp_directory_path() / "zilliqa" /
                         "updates" / manifest["uuid"].asString();
  const auto inputFilePath = updateDir / "zilliqa.tar.gz";

  boost::process::v2::process untarProcess{
      m_ioContext,
      "/usr/bin/tar",
      {"xfv", inputFilePath.string()},
      boost::process::v2::process_start_dir{updateDir.string()}};

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
  std::unique_lock<std::mutex> guard{m_mutex};
  try {
    m_updateState = {inputFilePath, false};
    m_pipe = std::make_unique<zil::UpdatePipe>(m_ioContext, zilliqPid,
                                               "zilliqad", "zilliqa");
    m_pipe->OnCommand =
        [this, quiesceDSBlock = manifest["quiesce-at-dsblock"].asString()](
            std::string_view cmd) { HandleReply(cmd, quiesceDSBlock); };
    m_pipe->Start();
    m_pipe->SyncWrite('|' + std::to_string(zilliqPid) + ',' +
                      manifest["quiesce-at-dsblock"].asString() + ',' +
                      manifest["upgrade-at-dsblock"].asString() + '|');
  } catch (...) {
    m_updateState = std::nullopt;
  }
}

bool ZilliqaUpdater::Update() {
  std::unique_lock<std::mutex> guard{m_mutex};

  if (!m_updateState) {
    LOG_GENERAL(WARNING, "No update is underway... ignoring");
    return false;
  }

  m_pipe->Stop();
  m_pipe.reset();

  if (!m_updateState->Acknowledged) {
    LOG_GENERAL(WARNING,
                "Update not acknowledged by zilliqa yet... cancelling");
    m_updateState = std::nullopt;
    return false;
  }

  auto srcFile = m_updateState->InputPath.parent_path() / "zilliqa";
  std::filesystem::path targetFile = "/usr/local/bin/zilliqa";
  std::filesystem::path backupFile = targetFile.string() + ".backup";

  // Create backup file
  std::error_code errorCode;
  std::filesystem::copy_file(targetFile, backupFile,
                             std::filesystem::copy_options::overwrite_existing,
                             errorCode);
  if (errorCode) {
    LOG_GENERAL(WARNING, "Update failed; couldn't create backup: "
                             << errorCode.message() << " (" << errorCode
                             << ") ... cancelling");
  } else {
    // Update zilliqa binary
    std::filesystem::copy_file(
        srcFile, targetFile, std::filesystem::copy_options::overwrite_existing,
        errorCode);

    if (errorCode) {
      // Copy failed; copy the backup back
      LOG_GENERAL(WARNING, "Update failed; couldn't copy file "
                               << srcFile << ": " << errorCode.message() << " ("
                               << errorCode << ") ... cancelling");

      std::filesystem::copy_file(
          backupFile, targetFile,
          std::filesystem::copy_options::overwrite_existing, errorCode);
      if (!errorCode) std::filesystem::remove(backupFile, errorCode);
    } else {
      // Success
      LOG_GENERAL(INFO, "Copied " << srcFile << " -> " << targetFile);

      // Cleanup
      std::filesystem::remove(targetFile, errorCode);
      std::filesystem::remove(backupFile, errorCode);
    }
  }

  m_updateState = std::nullopt;
  return true;
}

void ZilliqaUpdater::HandleReply(std::string_view cmd,
                                 const std::string& quiesceDSBlock) {
  std::unique_lock<std::mutex> guard{m_mutex};
  if (!m_updateState) return;

  auto first = cmd.find(",");
  if (first == std::string::npos || first + 3 != cmd.size()) return;

  std::size_t pos = 0;
  std::string s{cmd.data(), first};
  pid_t pid = std::stoi(s, &pos);
  if (pid != getpid() || pos != s.size()) {
    LOG_GENERAL(
        WARNING,
        "Ignoring invalid request from zilliqa from a different process");
    return;
  }

  if (cmd[first + 1] != 'O' || cmd[first + 2] != 'K') return;

  // TODO: parse
  LOG_GENERAL(INFO, "Update acknowledged.. waiting for zilliqa to shutdown at "
                        << quiesceDSBlock << " DS block");
  m_updateState->Acknowledged = true;
};
