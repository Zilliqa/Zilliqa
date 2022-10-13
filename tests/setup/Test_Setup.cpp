//
// Created by stephen on 13/10/22.
//

#define BOOST_TEST_MODULE setuptest
#define BOOST_TEST_DYN_LINK
#include <string>
#include <boost/filesystem/path.hpp>
#include <boost/test/unit_test.hpp>
#include "common/Constants.h"

BOOST_AUTO_TEST_SUITE(setuptest)

BOOST_AUTO_TEST_CASE(test_configuration) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Checking Configuration is correct for tests");

  BOOST_CHECK_EQUAL(false, LOOKUP_NODE_MODE);
  BOOST_CHECK_EQUAL(false, ENABLE_SCILLA_MULTI_VERSION);


  if (ENABLE_EVM) {
    boost::filesystem::path     evm_image(EVM_SERVER_BINARY);
    if (not boost::filesystem::exists(evm_image)) {
      LOG_GENERAL(WARNING,
                  "evm image does not seem to exist " << EVM_SERVER_BINARY);
    }
    if (not boost::filesystem::is_regular_file(EVM_SERVER_BINARY)) {
      LOG_GENERAL(WARNING,
                  "evm image is not a regular file " << EVM_SERVER_BINARY);
    }
  }
  if (ENABLE_EVM){
    boost::filesystem::path     scilla_image(SCILLA_ROOT);
    if (not boost::filesystem::exists(scilla_image)) {
      LOG_GENERAL(WARNING,
                  "scilla image does not seem to exist " << EVM_SERVER_BINARY);
    }
    if (not boost::filesystem::is_directory(EVM_SERVER_BINARY)) {
      LOG_GENERAL(WARNING,
                  "scilla root does not exist as a directory " << EVM_SERVER_BINARY);
    }

  }
  LOG_GENERAL(INFO, " ");
}


BOOST_AUTO_TEST_SUITE_END()
