#!/bin/bash
# This script is used for making image locally and pushing to private registry

# Usage:
#     ./scripts/make_image.sh         # using current HEAD
#     ./scripts/make_image.sh COMMIT  # using a specific commit

if [ ! -d .git ]
then
    echo "$0 should be run from project root directory"
    echo "Hint: run it like 'scripts/$(basename $0)'"
    exit 1
fi

commit=$1

if [ -z "$commit" ]
then
    commit=$(git rev-parse HEAD)
fi

echo "Making images using commit $commit"

github_commit=https://github.com/Zilliqa/Zilliqa/commit/$commit

curl -sf $github_commit > /dev/null

if [ $? != 0 ]
then
    echo "Checking '$github_commit' failed. Have you pushed the commit '$commit'?"
    exit 1
fi

TRAVIS_COMMIT=$commit ./scripts/ci_make_image.sh
