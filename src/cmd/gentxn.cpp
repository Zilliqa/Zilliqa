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

#include <boost/filesystem.hpp>
#include <climits>
#include <fstream>
#include <string>
#include <vector>

#include <boost/program_options.hpp>

#include <Schnorr.h>
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

namespace po = boost::program_options;

#define SUCCESS 0
#define ERROR_IN_COMMAND_LINE -1
#define ERROR_UNHANDLED_EXCEPTION -2
#define ERROR_UNEXPECTED -3

using KeyPairAddress = std::tuple<PrivKey, PubKey, Address>;
using NonceRange = std::tuple<std::size_t, std::size_t>;

std::vector<KeyPairAddress> get_genesis_keypair_and_address() {
  std::vector<KeyPairAddress> result;

  dev::strings allGenesis;
  allGenesis.reserve(GENESIS_KEYS.size() +
                     DS_GENESIS_KEYS.size());  // preallocate memory
  allGenesis.insert(allGenesis.end(), GENESIS_KEYS.begin(), GENESIS_KEYS.end());
  allGenesis.insert(allGenesis.end(), DS_GENESIS_KEYS.begin(),
                    DS_GENESIS_KEYS.end());

  for (auto& privKeyHexStr : allGenesis) {
    bytes out;
    if (DataConversion::HexStrToUint8Vec(privKeyHexStr, out)) {
      const auto& privKeyBytes{out};
      auto privKey = PrivKey{privKeyBytes, 0};
      auto pubKey = PubKey{privKey};
      auto address = Account::GetAddressFromPublicKey(pubKey);
      result.push_back(
          std::tuple<PrivKey, PubKey, Address>(privKey, pubKey, address));
    } else {
      LOG_GENERAL(WARNING, "Failed to get genesis key");
    }
  }

  return result;
}

void gen_txn_file(const std::string& prefix, const KeyPairAddress& from,
                  const Address& toAddr, const NonceRange& nonce_range) {
  const auto& privKey = std::get<0>(from);
  const auto& pubKey = std::get<1>(from);
  const auto& address = std::get<2>(from);

  const auto& begin = std::get<0>(nonce_range);
  const auto& end = std::get<1>(nonce_range);

  std::ostringstream oss;
  oss << prefix << "/" << address.hex() << "_" << begin << ".zil";
  // TODO: use address_being_end.zil as the following
  // oss << prefix << "/" << address.hex() << "_" << begin << "_" << end <<
  // ".zil";

  std::string txn_filename(oss.str());
  std::ofstream txn_file(txn_filename, std::fstream::binary);

  bytes txnBuff;
  std::vector<uint32_t> txnOffsets;

  for (auto nonce = begin; nonce < end; nonce++) {
    Transaction txn{DataConversion::Pack(CHAIN_ID, TRANSACTION_VERSION),
                    nonce,
                    toAddr,
                    std::make_pair(privKey, pubKey),
                    nonce,
                    GAS_PRICE_MIN_VALUE,
                    NORMAL_TRAN_GAS,
                    {},
                    {}};
    txnOffsets.push_back(txnBuff.size());
    if (!Messenger::SetTransaction(txnBuff, txnBuff.size(), txn)) {
      std::cout << "Messenger::SetTransaction failed." << std::endl;
      return;
    }
  }

  // The number of txn offset is number of transaction + 1.
  // So it is easiler to get the transaction size when read out.
  txnOffsets.push_back(txnBuff.size());

  bytes txnOffsetBuff;
  if (!Messenger::SetTransactionFileOffset(txnOffsetBuff, 0, txnOffsets)) {
    std::cout << "Messenger::SetTransactionFileOffset failed." << std::endl;
    return;
  }

  bytes buf;
  SerializableDataBlock::SetNumber<uint32_t>(buf, 0, txnOffsetBuff.size(),
                                             sizeof(uint32_t));
  buf.insert(buf.end(), txnOffsetBuff.begin(), txnOffsetBuff.end());
  buf.insert(buf.end(), txnBuff.begin(), txnBuff.end());

  txn_file.write(reinterpret_cast<char*>(buf.data()), buf.size());

  if (txn_file) {
    std::cout << "Write to file " << txn_filename << "\n";
  } else {
    std::cerr << "Error writing to file " << txn_filename << "\n";
  }
}

using namespace std;

void description() {
  std::cout << endl << "Description:\n";
  std::cout
      << "\tGenerate transactions starting from batch BEGIN (default to 0) "
         "to batch END (default to START+10000)\n";
  std::cout
      << "\tTransaction are generated from genesis accounts (constants.xml) "
         "to one random wallet\n";
  std::cout << "\tThe batch size is decided by NUM_TXN_TO_SEND_PER_ACCOUNT "
               "(constants.xml)\n";
}

int main(int argc, char** argv) {
  try {
    const unsigned long delta = 10000;
    unsigned long begin = 0, end;

    po::options_description desc("Options");

    desc.add_options()("help,h", "Print help messages")(
        "begin, b", po::value<unsigned long>(&begin),
        "Start of transaction batch (default to 0)")(
        "end, e", po::value<unsigned long>(&end),
        "End of transaction batch (default to parameter value --begin + "
        "10000)");

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);

      if (vm.count("help")) {
        SWInfo::LogBrandBugReport();
        description();
        cout << desc << endl;
        return SUCCESS;
      }
      if (!vm.count("end")) {
        if (begin != ULONG_MAX) {
          end = begin + delta;
        }
      }
      if (begin == ULONG_MAX || end == ULONG_MAX || begin > end) {
        description();
        return 1;
      }

    } catch (boost::program_options::required_option& e) {
      SWInfo::LogBrandBugReport();
      cerr << "ERROR: " << e.what() << endl << endl;
      cout << desc;
      return ERROR_IN_COMMAND_LINE;
    } catch (boost::program_options::error& e) {
      SWInfo::LogBrandBugReport();
      cerr << "ERROR: " << e.what() << endl << endl;
      return ERROR_IN_COMMAND_LINE;
    }

    auto receiver = Schnorr::GenKeyPair();
    auto toAddr = Account::GetAddressFromPublicKey(receiver.second);

    std::string txn_path{TXN_PATH};
    if (!boost::filesystem::exists(txn_path)) {
      std::cerr << "Cannot find path '" << txn_path
                << "', check TXN_PATH in constants.xml\n";
      return 1;
    }

    auto batch_size = NUM_TXN_TO_SEND_PER_ACCOUNT;

    auto fromAccounts = get_genesis_keypair_and_address();

    std::cout << "Number of genesis accounts: " << fromAccounts.size() << "\n";
    std::cout << "Begin batch: " << begin << "\n";
    std::cout << "End batch: " << end << "\n";
    std::cout << "Destionation directory (TXN_PATH): " << txn_path << "\n";
    std::cout << "Batch size (NUM_TXN_TO_SEND_PER_ACCOUNT): " << batch_size
              << "\n";

    for (auto batch = begin; batch < end; batch++) {
      auto begin_nonce = batch * batch_size + 1;
      auto end_nonce = (batch + 1) * batch_size + 1;
      auto nonce_range = std::make_tuple(begin_nonce, end_nonce);

      for (auto& from : fromAccounts) {
        gen_txn_file(txn_path, from, toAddr, nonce_range);
      }
    }

  } catch (exception& e) {
    cerr << "Unhandled Exception reached the top of main: " << e.what()
         << ", application will now exit" << endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }
  return SUCCESS;
}
