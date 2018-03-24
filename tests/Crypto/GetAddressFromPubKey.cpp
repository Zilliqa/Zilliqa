#include <array>
#include <chrono>
#include <iostream>
#include <functional>
#include <thread>

#include <boost/multiprecision/cpp_int.hpp>

#include "libData/AccountData/Address.h"
#include "common/Serializable.h"
#include "common/Messages.h"
#include "common/Constants.h"
#include "libCrypto/Sha2.h"
#include "libCrypto/Schnorr.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

int main()
{
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Reset();
    vector<unsigned char> message;
    string s;
    cin>>s;
    // TODO: Handle Exceptions
    PubKey key(DataConversion::HexStrToUint8Vec(s), 0);
    key.Serialize(message, 0);
    sha2.Update(message, 0, PUB_KEY_SIZE);
    const vector<unsigned char> & tmp2 = sha2.Finalize();
    Address toAddr;
    copy(tmp2.end() - ACC_ADDR_SIZE, tmp2.end(), toAddr.asArray().begin());
    cout<<toAddr<<endl;
}