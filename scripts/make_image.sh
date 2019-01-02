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
