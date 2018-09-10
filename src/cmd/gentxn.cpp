/**
* Copyright (c) 2018 Zilliqa 
* This source code is being disclosed to you solely for the purpose of your participation in 
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
* the protocols and algorithms that are programmed into, and intended by, the code. You may 
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
* including modifying or publishing the code (or any part of it), and developing or forming 
* another public or private blockchain network. This source code is provided ‘as is’ and no 
* warranties are given as to title or non-infringement, merchantability or fitness for purpose 
* and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
* Some programs in this code are governed by the GNU General Public License v3.0 (available at 
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
* and which include a reference to GPLv3 in their program files.
**/

#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libUtils/Logger.h"
#include <boost/filesystem.hpp>
#include <climits>
#include <fstream>
#include <string>
#include <vector>

using KeyPairAddress = std::tuple<PrivKey, PubKey, Address>;
using NonceRange = std::tuple<std::size_t, std::size_t>;

std::vector<KeyPairAddress> get_genesis_keypair_and_address()
{
    std::vector<KeyPairAddress> result;

    for (auto& privKeyHexStr : GENESIS_KEYS)
    {
        auto privKeyBytes{DataConversion::HexStrToUint8Vec(privKeyHexStr)};
        auto privKey = PrivKey{privKeyBytes, 0};
        auto pubKey = PubKey{privKey};
        auto address = Account::GetAddressFromPublicKey(pubKey);

        result.push_back(
            std::tuple<PrivKey, PubKey, Address>(privKey, pubKey, address));
    }

    return result;
}

void gen_txn_file(const std::string& prefix, const KeyPairAddress& from,
                  const Address& toAddr, const NonceRange& nonce_range)
{
    const auto& privKey = std::get<0>(from);
    const auto& pubKey = std::get<1>(from);
    const auto& address = std::get<2>(from);

    const auto& begin = std::get<0>(nonce_range);
    const auto& end = std::get<1>(nonce_range);

    std::ostringstream oss;
    oss << prefix << "/" << address.hex() << "_" << begin << ".zil";
    // TODO: use address_being_end.zil as the following
    // oss << prefix << "/" << address.hex() << "_" << begin << "_" << end << ".zil";

    std::string txn_filename(oss.str());
    ofstream txn_file(txn_filename, std::fstream::binary);

    std::vector<unsigned char> buf;

    for (auto nonce = begin; nonce < end; nonce++)
    {

        Transaction txn{0, nonce, toAddr, make_pair(privKey, pubKey), nonce, 1,
                        1, {},    {}};

        txn.Serialize(buf, 0);
        txn_file.write(reinterpret_cast<char*>(buf.data()), buf.size());
    }

    if (txn_file)
    {
        std::cout << "Write to file " << txn_filename << "\n";
    }
    else
    {
        std::cerr << "Error writing to file " << txn_filename << "\n";
    }
}

void usage(const std::string& prog)
{
    cout << "Usage: " << prog << " [BEGIN [END]]\n";
    cout << "\n";
    cout << "Description:\n";
    cout << "\tGenerate transactions starting from batch BEGIN (default to 0) "
            "to batch END (default to START+10000)\n";
    cout << "\tTransaction are generated from genesis accounts (constants.xml) "
            "to one random wallet\n";
    cout << "\tThe batch size is decided by NUM_TXN_TO_SEND_PER_ACCOUNT "
            "(constants.xml)\n";
}

int main(int argc, char** argv)
{
    string prog(argv[0]);

    const unsigned long delta = 10000;
    unsigned long begin = 0, end = delta;

    if (argc > 1)
    {
        begin = strtoul(argv[1], nullptr, 10);
        if (begin != ULONG_MAX)
        {
            end = begin + delta;
        }
    }

    if (argc > 2)
    {
        end = strtoul(argv[2], nullptr, 10);
    }

    if (begin == ULONG_MAX || end == ULONG_MAX || begin > end)
    {
        usage(prog);
        return 1;
    }

    auto receiver = Schnorr::GetInstance().GenKeyPair();
    auto toAddr = Account::GetAddressFromPublicKey(receiver.second);

    std::string txn_path{TXN_PATH};
    if (!boost::filesystem::exists(txn_path))
    {
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

    for (auto batch = begin; batch < end; batch++)
    {
        auto begin_nonce = batch * batch_size;
        auto end_nonce = (batch + 1) * batch_size;
        auto nonce_range = std::make_tuple(begin_nonce, end_nonce);

        for (auto& from : fromAccounts)
        {
            gen_txn_file(txn_path, from, toAddr, nonce_range);
        }
    }
}
