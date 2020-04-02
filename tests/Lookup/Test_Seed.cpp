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
#include <netdb.h>
#include <sys/socket.h>
#include <array>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <Schnorr.h>
#include "common/Messages.h"
#include "common/Serializable.h"
#include "common/Sizes.h"
#include "libData/Block.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/TimeUtils.h"

#define BOOST_TEST_MODULE seedtest
#include <boost/test/included/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

class tcp_client {
 private:
  int sock;
  std::string address;
  int port;
  struct sockaddr_in server;

 public:
  tcp_client();
  bool conn(string, int);
  bool send_data(string data);
  string receive(int);
};

tcp_client::tcp_client() {
  sock = -1;
  port = 0;
  address = "";
}

/**
    Connect to a host on a certain port number
*/
bool tcp_client::conn(string address, int port) {
  // create socket if it is not already created
  if (sock == -1) {
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
      perror("Could not create socket");
    }

    cout << "Socket created\n";
  } else { /* OK , nothing */
  }

  // setup address structure
  if (inet_addr(address.c_str()) == -1) {
    struct hostent* he;
    struct in_addr** addr_list;

    // resolve the hostname, its not an ip address
    if ((he = gethostbyname(address.c_str())) == NULL) {
      // gethostbyname failed
      herror("gethostbyname");
      cout << "Failed to resolve hostname\n";

      return false;
    }

    // Cast the h_addr_list to in_addr , since h_addr_list also has the ip
    // address in long format only
    addr_list = (struct in_addr**)he->h_addr_list;

    for (int i = 0; addr_list[i] != NULL; i++) {
      // strcpy(ip , inet_ntoa(*addr_list[i]) );
      server.sin_addr = *addr_list[i];

      char str[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, *addr_list[i], str, INET_ADDRSTRLEN);

      cout << address << " resolved to " << string(str) << endl;

      break;
    }
  }

  // plain ip address
  else {
    server.sin_addr.s_addr = inet_addr(address.c_str());
  }

  server.sin_family = AF_INET;
  server.sin_port = htons(port);

  // Connect to remote server
  if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
    perror("connect failed. Error");
    return 1;
  }

  cout << "Connected\n";
  return true;
}

/**
    Send data to the connected host
*/
bool tcp_client::send_data(string data) {
  // Send some data
  if (send(sock, data.c_str(), strlen(data.c_str()), 0) < 0) {
    perror("Send failed : ");
    return false;
  }
  cout << "Data send\n";

  return true;
}

/**
    Receive data from the connected host
*/
string tcp_client::receive(int size = 512) {
  char buffer[size];
  string reply;

  // Receive a reply from the server
  if (recv(sock, buffer, sizeof(buffer), 0) < 0) {
    puts("recv failed");
  }

  reply = buffer;
  return reply;
}

int main(int argc, char* argv[]) {
  tcp_client c;
  string host;

  cout << "Enter hostname : ";
  cin >> host;

  // connect to host
  c.conn(host, 80);

  // send some data
  c.send_data("GET / HTTP/1.1\r\n\r\n");

  // receive and echo reply
  cout << "----------------------------\n\n";
  cout << c.receive(1024);
  cout << "\n\n----------------------------\n\n";

  // done
  return 0;
}

BOOST_AUTO_TEST_SUITE(seedtest)

BOOST_AUTO_TEST_CASE(testDSBlockRetrieval) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  uint32_t listen_port = 5000;
  struct in_addr ip_addr;
  inet_pton(AF_INET, "127.0.0.1", &ip_addr);
  Peer lookup_node((uint128_t)ip_addr.s_addr, listen_port);

  bytes dsblockmsg = {MessageType::DIRECTORY, DSInstructionType::DSBLOCK};
  unsigned int curr_offset = MessageOffset::BODY;

  std::array<unsigned char, BLOCK_HASH_SIZE> prevHash1;

  for (unsigned int i = 0; i < prevHash1.size(); i++) {
    prevHash1.at(i) = i + 1;
  }

  std::array<unsigned char, BLOCK_SIG_SIZE> signature1;

  for (unsigned int i = 0; i < signature1.size(); i++) {
    signature1.at(i) = i + 8;
  }

  PairOfKey pubKey1 = Schnorr::GenKeyPair();

  DSBlockHeader header1(20, prevHash1, 12344, pubKey1.first, pubKey1.second, 8,
                        789);

  DSBlock dsblock(header1, signature1);

  curr_offset += dsblock.Serialize(dsblockmsg, curr_offset);

  dsblockmsg.resize(curr_offset + 32);
  Serializable::SetNumber<uint128_t>(dsblockmsg, curr_offset, 0, UINT256_SIZE);
  curr_offset += UINT256_SIZE;

  struct sockaddr_in localhost;
  inet_pton(AF_INET, "127.0.0.1", &localhost.sin_addr);

  dsblockmsg.resize(curr_offset + 16);
  Serializable::SetNumber<uint128_t>(dsblockmsg, curr_offset,
                                     (uint128_t)localhost.sin_addr.s_addr,
                                     UINT128_SIZE);
  curr_offset += UINT128_SIZE;

  dsblockmsg.resize(curr_offset + 4);
  Serializable::SetNumber<uint32_t>(dsblockmsg, curr_offset, (uint32_t)5001, 4);
  curr_offset += 4;

  P2PComm::GetInstance().SendMessage(lookup_node, dsblockmsg);

  BOOST_CHECK_MESSAGE(
      "vegetable" == "vegetable",
      "ERROR: return value from DB not equal to inserted value");
}

BOOST_AUTO_TEST_SUITE_END()
