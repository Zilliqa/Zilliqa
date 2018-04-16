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

#include <arpa/inet.h>
#include <boost/multiprecision/cpp_int.hpp>
#include <iostream>
#include <iterator>
#include <netinet/in.h>
#include <sys/socket.h>

#include "common/Constants.h"
#include "common/Serializable.h"
#include "libNetwork/Peer.h"

#define BOOST_TEST_MODULE utils
#include <boost/test/included/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(utils)

BOOST_AUTO_TEST_CASE(testBoostBigNum)
{
    using namespace boost::multiprecision;

    uint256_t num = 256;

    // Arithmetic ops
    num++;
    cout << num << endl;
    num--;
    cout << num << endl;
    num = num + 1;
    cout << num << endl;
    num = num + num;
    cout << num << endl;
    num *= 2;
    cout << num << endl;

    // Logical ops
    cout << (num >= num) << endl;
    cout << (num == 514) << endl;
    cout << (num != 514) << endl;

    // Bit ops
    num = num << 1;
    cout << num << endl;
    num = num >> 1;
    cout << num << endl;
    num = num ^ 0xFF;
    cout << num << endl;
    num = num & 0xFFFF;
    cout << num << endl;

    // Serialize
    vector<unsigned char> bytestream(32, 0x00);
    for (unsigned int i = 0; i < 32; i++)
    {
        bytestream.at(31 - i) = num.convert_to<uint8_t>();
        num >>= 8;
    }
    copy(bytestream.begin(), bytestream.end(),
         std::ostream_iterator<int>(cout, " "));
    cout << endl;

    // Deserialize
    uint256_t num2 = 0;
    for (unsigned int i = 0; i < 32; i++)
    {
        num2 = (num2 << 8) + bytestream.at(i);
    }
    cout << num2 << endl;

    struct in_addr ip_addr;
    inet_aton("54.169.197.255", &ip_addr);

    uint128_t ipaddr_big = ip_addr.s_addr;
    uint32_t ipaddr_normal = ip_addr.s_addr;

    vector<unsigned char> v1, v2;
    Serializable::SetNumber<uint128_t>(v1, 0, ipaddr_big, UINT128_SIZE);
    Serializable::SetNumber<uint32_t>(v2, 0, ipaddr_normal, sizeof(uint32_t));

    uint128_t ipaddr_big_2
        = Serializable::GetNumber<uint128_t>(v1, 0, UINT128_SIZE);
    auto ipaddr_normal_2
        = Serializable::GetNumber<uint32_t>(v2, 0, sizeof(uint32_t));

    cout << "ORIG BIG    = " << ipaddr_big << endl;
    cout << "DESE BIG    = " << ipaddr_big_2 << endl;

    cout << "ORIG NORMAL = " << ipaddr_normal << std::hex << endl;
    cout << "DESE NORMAL = " << ipaddr_normal_2 << endl;

    struct sockaddr_in serv_addr;
    serv_addr.sin_addr.s_addr = ipaddr_big_2.convert_to<uint32_t>();
    cout << "BIG    = " << inet_ntoa(serv_addr.sin_addr) << endl;

    serv_addr.sin_addr.s_addr = ipaddr_normal_2;
    cout << "NORMAL = " << inet_ntoa(serv_addr.sin_addr) << endl;
}

BOOST_AUTO_TEST_SUITE_END()
