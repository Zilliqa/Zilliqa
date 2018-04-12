#!/bin/bash

set -ev
set -x
unzip -o 3rdParty/gtest/gtest-1.7.0.zip -d 3rdParty/gtest


mkdir -p  build_travis
cd build_travis
cmake  -DCMAKE_CXX_FLAGS=-std=c++14 -DADD_G3LOG_UNIT_TEST=ON ..

makeArg=`grep -c ^processor /proc/cpuinfo`    

make -j$makeArg
/bin/bash ../scripts/runAllTests.sh

