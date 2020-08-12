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

dir=build

run_clang_format_fix=0
run_clang_tidy_fix=0

for option in "$@"
do
    case $option in
    cuda)
        CMAKE_EXTRA_OPTIONS="-DCUDA_MINE=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with CUDA"
    ;;
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
    fb)
        CMAKE_EXTRA_OPTIONS="-DFALLBACKTEST=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with Fallback test"
    ;;
    vc1)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_DS_SUSPEND_1=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test - Suspend DS leader for 1 time (before DS block consensus)"
    ;;
    vc2)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_DS_SUSPEND_3=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test - Suspend DS leader for 3 times (before DS block consensus)"
    ;;
    govvc2)
        CMAKE_EXTRA_OPTIONS="-DGOVVC_TEST_DS_SUSPEND_3=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with GOVVC test - Suspend DS leader for 3 times (before DS block consensus)"
    ;;
    vc3)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_FB_SUSPEND_1=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test - Suspend DS leader for 1 time (before Final block consensus)"
    ;;
    vc4)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_FB_SUSPEND_3=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test - Suspend DS leader for 3 times (before Final block consensus)"
    ;;
    vc5)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_VC_SUSPEND_1=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test - Suspend DS leader for 1 time (before VC block consensus)"
    ;;
    vc6)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_VC_SUSPEND_3=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test - Suspend DS leader for 3 times (before VC block consensus)"
    ;;
    vc7)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_VC_PRECHECK_1=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test - Caused the node to lag behind at ds epoch"
    ;;
    vc8)
        CMAKE_EXTRA_OPTIONS="-DVC_TEST_VC_PRECHECK_2=1 ${CMAKE_EXTRA_OPTIONS}"
        echo "Build with VC test - Caused the node to lag behind at tx epoch"
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
    *)
        echo "Usage $0 [cuda|opencl] [tsan|asan] [style] [heartbeattest] [fallbacktest] [vc<1-8>] [dm<1-9>] [sj<1-2>]"
        exit 1
    ;;
    esac
done

cmake -H. -B${dir} ${CMAKE_EXTRA_OPTIONS} -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTESTS=ON -DCMAKE_INSTALL_PREFIX=..
cmake --build ${dir} -- -j4
./scripts/license_checker.sh
./scripts/depends/check_guard.sh
[ ${run_clang_tidy_fix} -ne 0 ] && cmake --build ${dir} --target clang-tidy-fix
[ ${run_clang_format_fix} -ne 0 ] && cmake --build ${dir} --target clang-format-fix
