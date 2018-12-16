/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#ifndef SRC_LIBUTILS_INPUTDATAPROCESSING_H_
#define SRC_LIBUTILS_INPUTDATAPROCESSING_H_

#include <arpa/inet.h>
#include <iostream>

using std::cout;
using std::endl;

bool getIP(const char* in, struct in_addr& ip_addr) {
  int res = inet_pton(AF_INET, in, &ip_addr);
  if (!res) {
    res = inet_pton(AF_INET6, in, &ip_addr);
  }

  if (res == 1) {
    return true;
  } else if (res == 0) {
    cout << "Copyright (C) Zilliqa. Version 1.0 (Durian). "
            "<https://www.zilliqa.com/> "
         << endl;
    cout << "For bug reporting, please create an issue at "
            "<https://github.com/Zilliqa/Zilliqa> \n"
         << endl;
    cout << "Error: listen_ip_address does not contain a character string "
            "representing a valid network addres\n"
         << endl;
    return false;
  } else {
    cout << "Copyright (C) Zilliqa. Version 1.0 (Durian). "
            "<https://www.zilliqa.com/> "
         << endl;
    cout << "For bug reporting, please create an issue at "
            "<https://github.com/Zilliqa/Zilliqa> \n"
         << endl;
    cout << "Internal Error: cannot process the input IP address.\n" << endl;
    return false;
  }
}

#endif /* SRC_LIBUTILS_INPUTDATAPROCESSING_H_ */
