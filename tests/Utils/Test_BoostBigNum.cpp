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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <iostream>
#include <iterator>

#include "common/Constants.h"
#include "common/Serializable.h"
#include "libNetwork/Peer.h"

#define BOOST_TEST_MODULE utils
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(utils)

BOOST_AUTO_TEST_CASE(testBoostBigNum) {
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
  bytes bytestream(32, 0x00);
  for (unsigned int i = 0; i < 32; i++) {
    bytestream.at(31 - i) = num.convert_to<uint8_t>();
    num >>= 8;
  }
  copy(bytestream.begin(), bytestream.end(),
       std::ostream_iterator<int>(cout, " "));
  cout << endl;

  // Deserialize
  uint256_t num2 = 0;
  for (unsigned int i = 0; i < 32; i++) {
    num2 = (num2 << 8) + bytestream.at(i);
  }
  cout << num2 << endl;

  struct in_addr ip_addr {};
  inet_pton(AF_INET, "54.169.197.255", &ip_addr);

  uint128_t ipaddr_big = ip_addr.s_addr;
  uint32_t ipaddr_normal = ip_addr.s_addr;

  bytes v1, v2;
  Serializable::SetNumber<uint128_t>(v1, 0, ipaddr_big, UINT128_SIZE);
  Serializable::SetNumber<uint32_t>(v2, 0, ipaddr_normal, sizeof(uint32_t));

  uint128_t ipaddr_big_2 =
      Serializable::GetNumber<uint128_t>(v1, 0, UINT128_SIZE);
  uint32_t ipaddr_normal_2 =
      Serializable::GetNumber<uint32_t>(v2, 0, sizeof(uint32_t));

  cout << "ORIG BIG    = " << ipaddr_big << endl;
  cout << "DESE BIG    = " << ipaddr_big_2 << endl;

  cout << "ORIG NORMAL = " << ipaddr_normal << std::hex << endl;
  cout << "DESE NORMAL = " << ipaddr_normal_2 << endl;

  char big_ip[INET_ADDRSTRLEN];
  struct sockaddr_in serv_addr {};
  serv_addr.sin_addr.s_addr = ipaddr_big_2.convert_to<uint32_t>();
  inet_ntop(AF_INET, &(serv_addr.sin_addr), big_ip, INET_ADDRSTRLEN);
  cout << "BIG    = " << string(big_ip) << endl;

  char normal_ip[INET_ADDRSTRLEN];
  serv_addr.sin_addr.s_addr = ipaddr_normal_2;
  inet_ntop(AF_INET, &(serv_addr.sin_addr), big_ip, INET_ADDRSTRLEN);
  cout << "NORMAL = " << string(normal_ip) << endl;
}

BOOST_AUTO_TEST_SUITE_END()
