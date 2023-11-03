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

if [ -z "${VCPKG_ROOT}" ]; then
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
    aarch*)     ARCH=arm64;;
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
build_type="RelWithDebInfo"

./scripts/license_checker.sh
./scripts/ci_xml_checker.sh constants.xml
./scripts/ci_xml_checker.sh constants_local.xml
if [ "$OS" != "osx" ]; then ./scripts/depends/check_guard.sh; fi

# Find the git tag if we can and include it so we can report it in our GetVersion call
commit_id=`git rev-parse HEAD | cut -c -8`
is_modified=`git status --porcelain=v1 2>/dev/null | wc -l`
if [ $is_modified != 0 ]; then
    commit_id="${commit_id}+"
fi
CMAKE_EXTRA_OPTIONS="-DCOMMIT_ID=\"${commit_id}\" ${CMAKE_EXTRA_OPTIONS}"

for option in "$@"
do
    case $option in
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
        echo "Build with libfuzzer"``
    ;;
    style)
        CMAKE_EXTRA_OPTIONS="-DLLVM_EXTRA_TOOLS=ON ${CMAKE_EXTRA_OPTIONS}"
        run_clang_format_fix=1
        echo "Build with LLVM Extra Tools for coding style check (clang-format-fix)"
    ;;
    nomark)
          CMAKE_EXTRA_OPTIONS="-DNOMARK=ON ${CMAKE_EXTRA_OPTIONS}"
          echo "Build with no markers"
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
    evm)
        echo "Build EVM"
        evm_build_result=$(cd evm-ds; cargo build --release)
	      exit "$evm_build_result"
    ;;
    ninja)
        CMAKE_EXTRA_OPTIONS="-G Ninja ${CMAKE_EXTRA_OPTIONS}"
        # Ninja is parallelized by default
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
if [ -z "${BUILD_DIR}" ]; then
  build_dir=build
else
  build_dir="${BUILD_DIR}"
fi

if [ -z "${INSTALL_DIR}" ]; then
  install_dir="${BUILD_DIR}/install"
else
  install_dir="${INSTALL_DIR}"
fi

echo "Current directory: $(pwd)"
echo "Build directory: ${build_dir}"
echo "Install directory: ${install_dir}"


echo building using $jobs jobs

cmake -H. -B"${build_dir}" ${CMAKE_EXTRA_OPTIONS}  -DCMAKE_BUILD_TYPE=${build_type} -DCMAKE_INSTALL_PREFIX="${install_dir}" -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}"/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=${VCPKG_TRIPLET}
cmake --build "${build_dir}" --config ${build_type} -j $jobs

if command -v ccache &> /dev/null; then
  echo "ccache status"
  ccache -s
fi

if [ ${run_clang_tidy_fix} -ne 0 ]; then cmake --build "${build_dir}" --config ${build_type} --target clang-tidy-fix; fi
if [ ${run_clang_format_fix} -ne 0 ]; then cmake --build "${build_dir}" --config ${build_type} --target clang-format-fix; fi
if [ ${run_code_coverage} -ne 0 ]; then cmake --build "${build_dir}" --config ${build_type} --target Zilliqa_coverage; fi
