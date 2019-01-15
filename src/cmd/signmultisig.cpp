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

#include <iostream>

#include <boost/program_options.hpp>

#include "libCrypto/MultiSig.h"
#include "libUtils/SWInfo.h"

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_UNHANDLED_EXCEPTION -2

using namespace std;
namespace po = boost::program_options;

int main(int argc, const char* argv[]) {
  string message_;
  string privk_fn;
  string pubk_fn;
  vector<PubKey> pubKeys;
  vector<PrivKey> privKeys;

  try {
    po::options_description desc("Options");

    desc.add_options()("help,h", "Print help messages")(
        "message,m", po::value<string>(&message_)->required(),
        "Message string in hexadecimal format")(
        "privk,i", po::value<string>(&privk_fn)->required(),
        "Filename containing private keys each per line")(
        "pubk,u", po::value<string>(&pubk_fn)->required(),
        "Filename containing public keys each per line");

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);

      if (vm.count("help")) {
        SWInfo::LogBrandBugReport();
        cout << desc << endl;
        return SUCCESS;
      }
      po::notify(vm);
    } catch (boost::program_options::required_option& e) {
      SWInfo::LogBrandBugReport();
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      std::cout << desc;
      return ERROR_IN_COMMAND_LINE;
    } catch (boost::program_options::error& e) {
      SWInfo::LogBrandBugReport();
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      return ERROR_IN_COMMAND_LINE;
    }

    bytes message(message_.begin(), message_.end());

    vector<uint8_t> v;
    v.push_back(53);

    PrivKey pk(v, 0);

    string line;
    try {
      bytes key_v;
      fstream privFile(privk_fn, ios::in);
      while (getline(privFile, line)) {
        try {
          privKeys.push_back(PrivKey::GetPrivKeyFromString(line));
        } catch (std::invalid_argument& e) {
          std::cerr << e.what() << endl;
          return ERROR_IN_COMMAND_LINE;
        }
      }
    } catch (std::exception& e) {
      std::cerr << "Problem occured when processing private keys on line: "
                << privKeys.size() + 1 << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    if (privKeys.size() < 1) {
      std::cerr << "No private keys loaded" << endl;
      std::cerr << "Empty or corrupted or missing file: " << privk_fn << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    try {
      bytes key_v;
      fstream pubFile(pubk_fn, ios::in);
      while (getline(pubFile, line)) {
        try {
          pubKeys.push_back(PubKey::GetPubKeyFromString(line));
        } catch (std::invalid_argument& e) {
          std::cerr << e.what() << endl;
          return ERROR_IN_COMMAND_LINE;
        }
      }
    } catch (std::exception& e) {
      std::cerr << "Problem occured when processing public keys on line: "
                << pubKeys.size() + 1 << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    if (pubKeys.size() < 1) {
      std::cerr << "No public keys loaded" << endl;
      std::cerr << "Empty or corrupted or missing file: " << pubk_fn << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    if (privKeys.size() != pubKeys.size()) {
      cout << "Private key number must equal to public key number!";
      return -1;
    }

    /// Aggregate public keys
    shared_ptr<PubKey> aggregatedPubkey = MultiSig::AggregatePubKeys(pubKeys);

    /// Generate individual commitments
    vector<CommitSecret> secrets(pubKeys.size());
    vector<CommitPoint> points;
    vector<CommitSecret> secrets1;
    vector<CommitPoint> points1;
    for (unsigned int i = 0; i < pubKeys.size(); i++) {
      bytes tmp1, tmp2;
      secrets.at(i).Serialize(tmp1, 0);
      secrets1.emplace_back(tmp1, 0);
      points.emplace_back(secrets.at(i));
      points.back().Serialize(tmp2, 0);
      points1.emplace_back(tmp2, 0);
    }

    /// Check PrintPoint function
    Schnorr::GetInstance().PrintPoint(aggregatedPubkey->m_P.get());

    /// Check CommitSecret operator =
    CommitSecret dummy_secret;
    dummy_secret = secrets.at(0);

    /// Aggregate commits
    shared_ptr<CommitPoint> aggregatedCommit =
        MultiSig::AggregateCommits(points);
    shared_ptr<CommitPoint> aggregatedCommit1 =
        MultiSig::AggregateCommits(points1);

    /// Generate challenge
    Challenge challenge(*aggregatedCommit, *aggregatedPubkey, message);
    bytes tmp;
    challenge.Serialize(tmp, 0);
    Challenge challenge2(tmp, 0);
    tmp.clear();

    /// Generate responses
    vector<Response> responses;
    vector<Response> responses1;
    for (unsigned int i = 0; i < pubKeys.size(); i++) {
      responses.emplace_back(secrets.at(i), challenge, privKeys.at(i));
      bytes tmp;
      responses.back().Serialize(tmp, 0);
      responses1.emplace_back(tmp, 0);
    }

    /// Aggregate responses
    shared_ptr<Response> aggregatedResponse =
        MultiSig::AggregateResponses(responses);
    shared_ptr<Response> aggregatedResponse1 =
        MultiSig::AggregateResponses(responses1);

    /// Generate the aggregated signature
    shared_ptr<Signature> signature =
        MultiSig::AggregateSign(challenge, *aggregatedResponse);

    bytes result;
    signature->Serialize(result, 0);

    std::string output;
    if (!DataConversion::Uint8VecToHexStr(result, output)) {
      SWInfo::LogBrandBugReport();
      std::cerr << "Failed signature conversion" << endl;
      return -1;
    }

    cout << output;
  } catch (std::exception& e) {
    std::cerr << "Unhandled Exception reached the top of main: " << e.what()
              << ", application will now exit" << std::endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }
  return SUCCESS;
}
