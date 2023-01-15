#!/bin/bash
# Copyright (C) 2019 Zilliqa
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

set -e

if [ -z ${VCPKG_ROOT} ]; then
  echo -e "\033[1;33mVCPKG_ROOT is not set\033[0m"
  exit 1
fi

# Determine vcpkg os and arch triplet
OS="unknown"
ARCH="$(uname -m)"

unameOut="$(uname -s)"
case "${unameOut}" in
    Linux*)     OS=linux;;
    Darwin*)    OS=osx;;
    *)          echo "Unknown machine ${unameOut}"
esac

unameOut="$(uname -m)"
case "${unameOut}" in
    arm*)       ARCH=arm64;;
    x86_64*)    ARCH=x64;;
    *)          echo "Unknown machine ${unameOut}"
esac

VCPKG_TRIPLET=${ARCH}-${OS}-dynamic

# set n_parallel to fully utilize the resources
os=$(uname)
case $os in
    'Linux')
        n_parallel=$(nproc)
        ;;
    'Darwin')
        n_parallel=$(sysctl -n hw.ncpu)
        ;;
    *)
        n_parallel=2
        ;;
esac

echo "n_parallel=${n_parallel}"

if ! command -v ccache &> /dev/null; then
  echo "ccache isn't installed"
else
  echo "ccache configuration"
  ccache --version
  ccache -p

  ccache -z
  echo "ccache status"
  ccache -s
fi

run_clang_format_fix=0
run_clang_tidy_fix=0
run_code_coverage=0
parallelize=1
build_type="Debug"

./scripts/license_checker.sh
./scripts/ci_xml_checker.sh constants.xml
./scripts/ci_xml_checker.sh constants_local.xml
if [ "$OS" != "osx" ]; then ./scripts/depends/check_guard.sh; fi

for option in "$@"
do
    case $option in
    opencl)
        CMAKE_EXTRA_OPTIONS="-DOPENCL_MINE=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with OpenCL"
    ;;
    tsan)
        CMAKE_EXTRA_OPTIONS="-DTHREAD_SANITIZER=ON ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with ThreadSanitizer"
    ;;
    asan)
        CMAKE_EXTRA_OPTIONS="-DADDRESS_SANITIZER=ON ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with AddressSanitizer"
    ;;
    undef)
        CMAKE_EXTRA_OPTIONS="-DUNDEF_BEHAVIOR_SANITIZER=ON ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with UndefinedBehaviorSanitizer"
    ;;
    fuzzer)
        CMAKE_EXTRA_OPTIONS="-DLIBFUZZER=ON ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with libfuzzer"
    ;;
    style)
        CMAKE_EXTRA_OPTIONS="-DLLVM_EXTRA_TOOLS=ON ${CMAKE_EXTRA_OPTIONS}"
        run_clang_format_fix=1
        echo "Build with LLVM Extra Tools for coding style check (clang-format-fix)"
    ;;
    linter)
        CMAKE_EXTRA_OPTIONS="-DLLVM_EXTRA_TOOLS=ON ${CMAKE_EXTRA_OPTIONS}"
        run_clang_tidy_fix=1
        echo "Build with LLVM Extra Tools for linter check (clang-tidy-fix)"
    ;;
    heartbeattest)
        CMAKE_EXTRA_OPTIONS="-DHEARTBEATTEST=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with HeartBeat test"
    ;;
    vc1)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_DS_SUSPEND_1=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test DS Suspend 1 - Suspend DS leader for 1 time (before DS block consensus)"
    ;;
    vc2)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_DS_SUSPEND_3=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test DS Suspend 3 - Suspend DS leader for 3 times (before DS block consensus)"
    ;;
    govvc2)
        CMAKE_EXTRA_OPTIONS="-DGOVVC_TEST_DS_SUSPEND_3=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with GOVVC test - Suspend DS leader for 3 times (before DS block consensus)"
    ;;
    vc3)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_FB_SUSPEND_1=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test FB Suspend 1 - Suspend DS leader for 1 time (before Final block consensus)"
    ;;
    vc4)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_FB_SUSPEND_3=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test FB Suspend 3- Suspend DS leader for 3 times (before Final block consensus)"
    ;;
    vc5)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_VC_SUSPEND_1=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test VC Suspend 1 - Suspend DS leader for 1 time (before VC block consensus)"
    ;;
    vc6)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_VC_SUSPEND_3=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test VC Suspend 3 - Suspend DS leader for 3 times (before VC block consensus)"
    ;;
    vc7)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_VC_PRECHECK_1=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test VC Precheck 1 - Caused the node to lag behind at ds epoch"
    ;;
    vc8)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_VC_PRECHECK_2=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test VC Precheck 2 - Caused the node to lag behind at tx epoch"
    ;;
    vc9)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_FB_SUSPEND_RESPONSE=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test FB Suspend consensus at commit done 1 - Caused the node to lag behind at tx epoch"
    ;;
    dm1)
        CMAKE_EXTRA_OPTIONS="-DDM_TEST_DM_LESSTXN_ONE=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with DSMBMerging test - DS leader has some txn that one of the backups doesn't have"
    ;;
    dm2)
        CMAKE_EXTRA_OPTIONS="-DDM_TEST_DM_LESSTXN_ALL=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with DSMBMerging test - DS leader has some txn that all of backups don't have"
    ;;
    dm3)
        CMAKE_EXTRA_OPTIONS="-DDM_TEST_DM_LESSMB_ONE=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with DSMBMerging test - DS leader has more microblock received than one of the backups"
    ;;
    dm4)
        CMAKE_EXTRA_OPTIONS="-DDM_TEST_DM_LESSMB_ALL=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with DSMBMerging test - DS leader has more microblock received than all of the backups"
    ;;
    dm5)
        CMAKE_EXTRA_OPTIONS="-DDM_TEST_DM_BAD_ANNOUNCE=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with DSMBMerging test - DS leader composed invalid TxBlock"
    ;;
    dm6)
        CMAKE_EXTRA_OPTIONS="-DDM_TEST_DM_BAD_MB_ANNOUNCE=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with DSMBMerging test - DS leader composed invalid DSMicroBlock"
    ;;
    dm7)
        CMAKE_EXTRA_OPTIONS="-DDM_TEST_DM_MORETXN_LEADER=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with DSMBMerging test - DS leader doesn't have some txn"
    ;;
    dm8)
        CMAKE_EXTRA_OPTIONS="-DDM_TEST_DM_MORETXN_HALF=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with DSMBMerging test - DS leader and half of the DS doesn't have some txn"
    ;;
    dm9)
        CMAKE_EXTRA_OPTIONS="-DDM_TEST_DM_MOREMB_HALF=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with DSMBMerging test - DS leader and half of the DS doesn't have some microblock"
    ;;
    sj1)
        CMAKE_EXTRA_OPTIONS="-DSJ_TEST_SJ_TXNBLKS_PROCESS_SLOW=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with SJ test - New Seed take long time to process txnblocks during syncup"
    ;;
    sj2)
        CMAKE_EXTRA_OPTIONS="-DSJ_TEST_SJ_MISSING_MBTXNS=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with SJ test - New Seed misses the mbtxns message from multiplier"
    ;;
    evm)
        echo "Build EVM"
	evm_build_result=$(cd evm-ds; cargo build --release)
	exit $evm_build_result
    ;;
    ninja)
        CMAKE_EXTRA_OPTIONS="-G Ninja ${CMAKE_EXTRA_OPTIONS}"
        # Ninja is parallelized by default
        parallelize=0
        echo "Build using Ninja"
    ;;
    debug)
        build_type="Debug"
        echo "Build debug"
    ;;
    tests)
        CMAKE_EXTRA_OPTIONS="-DTESTS=ON ${CMAKE_EXTRA_OPTIONS}"
        echo "Build tests"
    ;;
    coverage)
        CMAKE_EXTRA_OPTIONS="-DLLVM_EXTRA_TOOLS=ON -DENABLE_COVERAGE=ON ${CMAKE_EXTRA_OPTIONS}"
        run_code_coverage=1
        echo "Build with code coverage"
    ;;
    *)
        echo "Usage $0 [opencl] [tsan|asan] [style] [heartbeattest] [vc<1-9>] [dm<1-9>] [sj<1-2>] [ninja] [debug]"
        exit 1
    ;;
    esac
done

# TODO: ideally these should be passed into the command line but at the
#       moment the script doesn't accept argument value so for simplicity
#       we use environment variables at the moment.
if [ -z ${BUILD_DIR} ]; then
  build_dir=build
else
  build_dir="${BUILD_DIR}"
fi

if [ -z ${INSTALL_DIR} ]; then
  install_dir="${BUILD_DIR}/install"
else
  install_dir="${INSTALL_DIR}"
fi

echo "Currenct directory: $(pwd)"
echo "Build directory: ${build_dir}"
echo "Install directory: ${install_dir}"

echo "nathan: doing this..."
cmake -H. -B"${build_dir}" ${CMAKE_EXTRA_OPTIONS} -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INSTALL_PREFIX="${install_dir}" -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=${VCPKG_TRIPLET}
if [ ${parallelize} -ne 0 ]; then
  cmake --build "${build_dir}" --config ${build_type} -j${n_parallel}
else
  cmake --build "${build_dir}" --config ${build_type}
fi

if command -v ccache &> /dev/null; then
  echo "ccache status"
  ccache -s
fi

if [ ${run_clang_tidy_fix} -ne 0 ]; then cmake --build "${build_dir}" --config ${build_type} --target clang-tidy-fix; fi
if [ ${run_clang_format_fix} -ne 0 ]; then cmake --build "${build_dir}" --config ${build_type} --target clang-format-fix; fi
if [ ${run_code_coverage} -ne 0 ]; then cmake --build "${build_dir}" --config ${build_type} --target Zilliqa_coverage; fi
