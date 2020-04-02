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

# Usage: ./scripts/license_checker.sh

banner_file=$(mktemp)

cat <<EOF > $banner_file

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
EOF
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
            ! -path "./src/depends/*" \
            ! -path "./scripts/depends/*")

lc_license=$(wc -l $banner_file | egrep "[0-9]+" -o | head -n1)

error_count=0

function check_license() {
    source=$1
    n_row_ignore=$2
    n_col_ignore=$3

    has_error=0
    cat $source | tail -n +$(($n_row_ignore + 1)) | head -n$lc_license | \
        cut -b $(($n_col_ignore+1))- | \
        diff -u $banner_file - 2>&1 >/dev/null || \
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
        cpp|hpp|tpp|h|c) check_license $file 2 3 ;;
        py|sh) check_license $file 2 2 ;;
        *) echo unsupported format;;
    esac
done

if [ $error_count -gt 0 ]
then
    echo
    echo "$error_count file(s) has incorrect LICENSE banner"
    exit 1
fi
