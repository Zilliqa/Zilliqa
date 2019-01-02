#!/bin/bash
# Copyright (c) 2018 Zilliqa
# This source code is being disclosed to you solely for the purpose of your
# participation in testing Zilliqa. You may view, compile and run the code for
# that purpose and pursuant to the protocols and algorithms that are programmed
# into, and intended by, the code. You may not do anything else with the code
# without express permission from Zilliqa Research Pte. Ltd., including
# modifying or publishing the code (or any part of it), and developing or
# forming another public or private blockchain network. This source code is
# provided 'as is' and no warranties are given as to title or non-infringement,
# merchantability or fitness for purpose and, to the extent permitted by law,
# all liability for your use of the code is disclaimed. Some programs in this
# code are governed by the GNU General Public License v3.0 (available at
# https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
# are governed by GPLv3.0 are those programs that are located in the folders
# src/depends and tests/depends and which include a reference to GPLv3 in their
# program files.
# This script is used for making image locally and pushing to private registry

# Usage:
#     ./scripts/make_image.sh         # using current HEAD
#     ./scripts/make_image.sh COMMIT  # using a specific commit

cmd=$0

function usage() {
cat <<EOF
Usage: $cmd [COMMIT]

Positional arguments:
    COMMIT                  The commit used to build zilliqa binaries. If it's
                            not set, the current HEAD will be used
Options:
    -h,--help               Show this help message

EOF
}

commit=""

while [ $# -gt 0 ]
do
    opt=$1
    case $opt in
        -t|--target)
            echo "Warning: --target option has been deprecated"
            shift 2
        ;;
        -h|--help)
            usage
            exit 1
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
TRAVIS_COMMIT=$commit ./scripts/ci_make_image.sh
if [ "$?" -ne 0 ]
then
    echo "Making image failed"
    exit 1
fi
