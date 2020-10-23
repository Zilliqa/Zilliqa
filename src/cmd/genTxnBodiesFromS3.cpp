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

/// Should be run from a folder with constants.xml with LOOKUP_NODE_MODE set to
/// true

#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include "libData/AccountData/TransactionReceipt.h"
#include "libPersistence/BlockStorage.h"
#include "libServer/JSONConversion.h"
#include "libUtils/FileSystem.h"

using namespace std;

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_UNHANDLED_EXCEPTION -2
#define ERROR_DOWNLOADING_BACKUP -3

#define JSON_OUTPUT_FOLDER "txns_json"

namespace po = boost::program_options;

string GetAwsS3CpString(const string& source, const string& dest) {
  ostringstream ossS3Cmd;
  ossS3Cmd << "aws s3 cp " << source << " " << dest << " --recursive";
  return ossS3Cmd.str();
}

int main(int argc, char* argv[]) {
  string bucketName, backupFolderName, jsonOutputPath;
  bool saveToJsonFormat = false;
  po::options_description desc("Options");

  desc.add_options()("help,h", "Print help messages")(
      "bucket-name,b", po::value<string>(&bucketName)->required(),
      "S3 bucket name")("backupFolderName,f",
                        po::value<string>(&backupFolderName)->required(),
                        "backup folder name in S3")(
      "saveToJsonFormat,j", po::bool_switch(&saveToJsonFormat),
      "Save the txns in json format to file")(
      "jsonOutputPath,p",
      po::value<string>(&jsonOutputPath)
          ->default_value(bfs::current_path().string() + "/" +
                          JSON_OUTPUT_FOLDER),
      "Json folder path to store txns in json format");

  po::variables_map vm;
  bool err = false;

  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);

    /** --help option
     */
    if (vm.count("help")) {
      SWInfo::LogBrandBugReport();
      cout << desc << endl;
      return SUCCESS;
    }
    po::notify(vm);

    string remoteS3Path = "s3://" + bucketName + "/" + backupFolderName;
    string localBackupPath =
        bfs::current_path().string() + "/" + backupFolderName;
    // Download from S3
    if (!SysCommand::ExecuteCmd(
            SysCommand::WITHOUT_OUTPUT,
            GetAwsS3CpString(remoteS3Path, localBackupPath))) {
      LOG_GENERAL(WARNING, "Failed to download backup folder from S3 : s3://"
                               << bucketName << "/" << backupFolderName);
      return ERROR_DOWNLOADING_BACKUP;
    } else {
      LOG_GENERAL(DEBUG, "Backup folder downloaded successfully : ");
    }

    // create json output folder
    if (saveToJsonFormat) {
      if (!(boost::filesystem::exists(jsonOutputPath))) {
        if (!boost::filesystem::create_directories(jsonOutputPath)) {
          cerr << "Failed to created JSON output folder ! will skip creating "
                  "txns in json format"
               << endl;
          saveToJsonFormat = false;
        }
      }
    }

    // Loop through all files in backupFolderName and store to leveldb
    vector<string> listOfTxnFiles = getAllFilesInDir(localBackupPath);
    for (const auto& txns_filename : listOfTxnFiles) {
      ifstream infile;
      LOG_GENERAL(INFO, "Parsing " << txns_filename << endl);
      try {
        // Filename ends with "_<epochnum>"
        uint64_t epochNum = boost::lexical_cast<uint64_t>(
            txns_filename.substr(txns_filename.rfind('_') + 1));

        // Now read back file to see if the TransactionWithReceipt is good
        infile.open(txns_filename, ios::in | ios::binary);
        TxnHash r_txn_hash;
        bytes buff;

        // Get the txnHash length and raw bytes of txnHash itself
        size_t len;
        // Loop through each txn in file
        while (infile.read(reinterpret_cast<char*>(&len), sizeof(len))) {
          infile.read(reinterpret_cast<char*>(&r_txn_hash), len);

          // get the TxnReceipt length and raw bytes of TxnReceipt itself
          infile.read(reinterpret_cast<char*>(&len), sizeof(len));
          buff.clear();
          buff.resize(len);
          infile.read(reinterpret_cast<char*>(buff.data()), len);

          // Deserialize the TxnReceipt bytes
          TransactionWithReceipt r_tr;
          r_tr.Deserialize(buff, 0);

          if (r_tr.GetTransaction().GetTranID() != r_txn_hash) {
            LOG_CHECK_FAIL("Txn Receipt Hash", r_txn_hash,
                           r_tr.GetTransaction().GetTranID());
            err = true;
            continue;
          }

          BlockStorage::GetBlockStorage().PutTxBody(epochNum, r_txn_hash, buff);
          LOG_GENERAL(INFO, "Inserted Txn:" << r_txn_hash
                                            << " Epoch: " << epochNum << endl);
          if (saveToJsonFormat) {
            Json::Value v = JSONConversion::convertTxtoJson(r_tr);

            // create file with filename as "<<txnhash>>.json"
            ofstream ofile(jsonOutputPath + "/" + r_txn_hash.hex() + ".json");
            ofile << v.toStyledString();
            ofile.close();
          }
        }
      } catch (...) {
        clog << boost::current_exception_diagnostic_information() << endl;
        LOG_GENERAL(WARNING,
                    "Exception while reading file - " << txns_filename << endl);
        err = true;
      }
      infile.close();
    }
  } catch (boost::program_options::required_option& e) {
    SWInfo::LogBrandBugReport();
    cerr << "ERROR: " << e.what() << endl;
    cout << desc;
    return ERROR_IN_COMMAND_LINE;
  } catch (boost::program_options::error& e) {
    SWInfo::LogBrandBugReport();
    cerr << "ERROR: " << e.what() << endl;
    return ERROR_IN_COMMAND_LINE;
  }

  if (err) {
    cout << "FAILURE! Check log for errors" << endl;
  }
  cout << "SUCCESS!" << endl;

  return SUCCESS;
}
