#!/bin/bash
# This script is used for making image locally and pushing to private registry

# Usage:
#     ./scripts/make_image.sh         # using current HEAD
#     ./scripts/make_image.sh COMMIT  # using a specific commit

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
aws ecr get-login --no-include-email --region us-east-1 >/dev/null
if [ "$?" -ne 0 ]
then
    echo "Did you have latest 'aws-cli' installed and AWS access key configured?"
    exit 1
fi

# Handling commit
[ -z "$1" ] && commit=$(git rev-parse HEAD) || commit=$1
echo "Making images using commit $commit"

# Checking commit availability on Github repo
github_commit=https://github.com/Zilliqa/Zilliqa/commit/$commit
curl -sf $github_commit > /dev/null
if [ "$?" -ne 0 ]
then
    echo "Checking '$github_commit' failed. Have you pushed the commit '$commit'?"
    exit 1
fi

# set n_parallel to fully utilize the resources
case "$(uname)" in
    'Linux')
        jobs=$(nproc)
        ;;
    'Darwin')
        jobs=$(sysctl -n hw.ncpu)
        ;;
    *)
        jobs=2
        ;;
esac

# Making images and checking result
TRAVIS_COMMIT=$commit ./scripts/ci_make_image.sh $jobs
if [ "$?" -ne 0 ]
then
    echo "Making image failed"
    exit 1
fi
