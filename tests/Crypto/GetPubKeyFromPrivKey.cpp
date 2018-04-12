#include <array>
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>

#include <boost/multiprecision/cpp_int.hpp>

#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Address.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

// Usage: input the hex string of private key
int main()
{
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Reset();
    vector<unsigned char> message;
    string s;
    cin >> s;

    PrivKey privKey{DataConversion::HexStrToUint8Vec(s), 0};
    PubKey pubKey{privKey};

    cout << pubKey << endl;
}
