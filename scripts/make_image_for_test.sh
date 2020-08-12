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
# This script is used for making image locally and pushing to private registry

# Usage:
#
# - Build images for all the test scenarios
#     ./scripts/make_image_for_test.sh               # using current HEAD
#     ./scripts/make_image_for_test.sh COMMIT        # using a specific commit
#
# - Build the image for only one specific test scenario
#     ./scripts/make_image_for_test.sh -n vc2        # for vc2
#     ./scripts/make_image_for_test.sh -n vc2 COMMIT # for vc2 at a specific commit

Test_scenarios=("-DVC_TEST_DS_SUSPEND_3=1 "
                "-DGOVVC_TEST_DS_SUSPEND_3=1 "
                "-DVC_TEST_FB_SUSPEND_3=1 "
                "-DVC_TEST_DS_SUSPEND_1=1 -DVC_TEST_VC_SUSPEND_3=1 "
                "-DVC_TEST_FB_SUSPEND_3=1 -DVC_TEST_VC_SUSPEND_3=1 "
                "-DVC_TEST_VC_PRECHECK_1=1 "
                "-DVC_TEST_VC_PRECHECK_2=1 "
                "-DDM_TEST_DM_LESSTXN_ONE=1 "
                "-DDM_TEST_DM_LESSTXN_ALL=1 "
                "-DDM_TEST_DM_LESSMB_ONE=1 "
                "-DDM_TEST_DM_LESSMB_ALL=1 "
                "-DDM_TEST_DM_BAD_ANNOUNCE=1 "
                "-DDM_TEST_DM_BAD_MB_ANNOUNCE=1 "
                "-DDM_TEST_DM_MORETXN_LEADER=1 "
                "-DDM_TEST_DM_MORETXN_HALF=1 "
                "-DDM_TEST_DM_MOREMB_HALF=1 "
                "-DADDRESS_SANITIZER=ON "
                "-DTHREAD_SANITIZER=ON "
                "-DSJ_TEST_SJ_TXNBLKS_PROCESS_SLOW=1 "
                "-DSJ_TEST_SJ_MISSING_MBTXNS=1 " )

Test_scenarios_name=( "vc2" "govvc2" "vc4" "vc1vc6" "vc3vc6" "vc7" "vc8" "dm1" "dm2" "dm3" "dm4" "dm5" "dm6" "dm7" "dm8" "dm9" "asan" "tsan" "sj1" "sj2" )

cmd=$0

function usage() {
cat <<EOF
Usage: $cmd [OPTIONS] [COMMIT]

Positional arguments:
    COMMIT                  The commit used to build zilliqa binaries. If it's
                            not set, the current HEAD will be used
Options:
    -n,--name STRING        Only build a specific test
    -h,--help               Show this help message

EOF
}

commit=""
name=""

while [ $# -gt 0 ]
do
    opt=$1
    case $opt in
        -h|--help)
            usage
            exit 1
        ;;
        -n|--name)
            name=$2
            shift 2
        ;;
        -*|--*)
            echo "Unrecognized option $opt"
            echo
            usage
            exit 1
        ;;
        *)
            if [ -z "$commit" ]
            then
                commit=$opt
                shift 1
            else
                echo "At most one positional argument needed"
                echo ""
                usage
                exit 1
            fi
        ;;
    esac
done

[ -z "$commit" ] && commit=$(git rev-parse HEAD)

# Checking running directory
if [ ! -d .git ]
then
    echo "$0 should be run from project root directory"
    echo "Hint: run it like 'scripts/$(basename $0)'"
    exit 1
fi

# Checking permission
if [ "$EUID" -eq 0 ]
then
    echo "Please do not run as root"
    echo
    echo "If you see docker permission error previously, check if you have sudo-less docker."
    echo "See https://docs.docker.com/install/linux/linux-postinstall"
    exit 1
fi

# Checking dependencies
which docker >/dev/null 2>&1
if [ "$?" -ne 0 ]
then
    echo "Cannot find command 'docker'"
    exit 1
fi
which aws >/dev/null 2>&1
if [ "$?" -ne 0 ]
then
    echo "Cannot find command 'aws'. Did you install 'aws-cli'?"
    exit 1
fi
aws ecr get-login --no-include-email --region us-west-2 >/dev/null
if [ "$?" -ne 0 ]
then
    echo "Did you have latest 'aws-cli' installed and AWS access key configured?"
    exit 1
fi

# Handling commit
echo "Making images using commit '$commit'"

# Checking commit availability on Github repo
github_commit=https://github.com/Zilliqa/Zilliqa/commit/$commit
curl -sf $github_commit > /dev/null
if [ "$?" -ne 0 ]
then
    echo "Checking '$github_commit' failed. Have you pushed the commit '$commit'?"
    exit 1
fi

# Making images and checking result
for ((i=0;i<${#Test_scenarios[@]};i++))
do
    # skip the test when $name is non-empty and not matching
    if [ -n "$name" ] && [ "$name" != "${Test_scenarios_name[$i]}" ]
    then
        continue
    fi

    echo "Building test scenario ${Test_scenarios_name[$i]}"
    TEST_EXTRA_CMAKE_ARGS=${Test_scenarios[$i]} TEST_NAME="-${Test_scenarios_name[$i]}" TRAVIS_COMMIT=$commit ./scripts/ci_make_image.sh
    exit_code=$?
    if [ "$exit_code" -ne 0 ]
    then
        echo "Making image failed"
        exit $exit_code
    fi
done
