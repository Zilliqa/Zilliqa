# Install script for directory: /home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/depends

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/depends/common/cmake_install.cmake")
  include("/home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/depends/json_spirit/cmake_install.cmake")
  include("/home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/depends/libDatabase/cmake_install.cmake")
  include("/home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/depends/libethash/cmake_install.cmake")
  include("/home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/depends/libTrie/cmake_install.cmake")
  include("/home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/depends/minilzo/cmake_install.cmake")
  include("/home/junhao/Desktop/octcoin/other_br/production/zilliqa/src/depends/Sha3/cmake_install.cmake")

endif()

