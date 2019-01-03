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

#include <setjmp.h>
#include <array>
#include <string>

#include <signal.h>
#include "libData/DataStructures/CircularArray.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE circulararraytest
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_ALTERNATIVE_INIT_API

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

jmp_buf JumpBuffer;

BOOST_AUTO_TEST_SUITE(circulararraytest)

BOOST_AUTO_TEST_CASE(CircularArray_test) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  CircularArray<int> arr;

  std::size_t CAPACITY = 100;
  arr.resize(CAPACITY);
  BOOST_CHECK_MESSAGE(arr.capacity() == CAPACITY, "arr.capacity() != CAPACITY");

  arr.insert_new(arr.size(), 1);
  arr.insert_new(arr.size(), 2);

  BOOST_CHECK_MESSAGE(arr[0] == 1, "arr[0] != 1!");
  BOOST_CHECK_MESSAGE(arr[1] == 2, "arr[1] != 2!");

  BOOST_CHECK_MESSAGE(arr.back() == 2, "arr.back() != 2!");

  for (int i = 0; i < 100; i++) {
    arr.insert_new(arr.size(), 11);
  }

  BOOST_CHECK_MESSAGE(arr[101] == 11, "arr[101] != 2!");

  arr[101] = 12;
  BOOST_CHECK_MESSAGE(arr[101] == 12, "arr[101] != 12!");
  BOOST_CHECK_MESSAGE(arr[101] != 11, "arr[101] == 11!");

  int value = -1;
  arr.insert_new(102, value);
  BOOST_CHECK_MESSAGE(arr[102] == -1, "arr[102] != -1!");

  arr.insert_new(arr.size(), 2);
  BOOST_CHECK_MESSAGE(arr[103] == 2, "arr[103] != 2!");
}

BOOST_AUTO_TEST_CASE(CircularArray_Empty_Index_Test) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();
  CircularArray<int> arr;

  if (setjmp(JumpBuffer) == 0) {
    arr[1];
    BOOST_CHECK_MESSAGE(false,
                        "No complains about indexing empty CircularArray");
  }
}

BOOST_AUTO_TEST_CASE(CircularArray_Empty_Insert_new_Test) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();
  CircularArray<int> arr;

  if (setjmp(JumpBuffer) == 0) {
    arr.insert_new(102, -1);
    BOOST_CHECK_MESSAGE(
        false,
        "No complains about calling insert_new() on empty CircularArray");
  }
}

BOOST_AUTO_TEST_CASE(CircularArray_Empty_Back_Test) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();
  CircularArray<int> arr;

  if (setjmp(JumpBuffer) == 0) {
    arr.back();
    BOOST_CHECK_MESSAGE(
        false, "No complains about calling back() on empty CircularArray");
  }
}

BOOST_AUTO_TEST_SUITE_END()

namespace utf = boost::unit_test;

void signalHandler(int sig) {
  (void)sig;  // get rid of error: unused parameter
  longjmp(JumpBuffer, 1);
}

// int main(int argc, char* argv[], char* envp[])
int main(int argc, char* argv[]) {
  signal(SIGABRT, signalHandler);
  return utf::unit_test_main(init_unit_test, argc, argv);
}
