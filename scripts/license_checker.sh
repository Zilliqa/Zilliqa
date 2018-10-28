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

# Usage: ./scripts/license_checker.sh

set -e

scope=$(find . -type f \( \
            -name "*.cpp" \
            -o -name "*.hpp" \
            -o -name "*.tpp" \
            -o -name "*.c" \
            -o -name "*.h" \
            -o -name "*.py" \
            -o -name "*.sh" \) \
            ! -name "run-clang-*.py" \
            ! -path "./build/*" \
            ! -path "./src/depends/*")

license_file=LICENSE

lc_license=$(wc -l $license_file | egrep "[0-9]+" -o)

error_count=0

function check_license() {
    source=$1
    n_row_ignore=$2
    n_col_ignore=$3

    has_error=0
    cat $source | tail -n +$(($n_row_ignore + 1)) | head -n$lc_license | \
        cut -b $(($n_col_ignore+1))- | \
        diff -u $license_file - 2>&1 >/dev/null || \
        has_error=1

    if [ $has_error -ne 0 ]
    then
        echo $source
        error_count=$(($error_count+1))
    fi
}

for file in $scope
do
    filename=$(basename $file)
    ext="${filename##*.}"
    case "$ext" in
        cpp|hpp|tpp|h|c) check_license $file 1 3 ;;
        py|sh) check_license $file 1 2 ;;
        *) echo unsupported format;;
    esac
done

if [ $error_count -gt 0 ]
then
    echo
    echo "$error_count file(s) has incorrect LICENSE banner"
    exit 1
fi
