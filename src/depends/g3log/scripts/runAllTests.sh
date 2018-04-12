#!/bin/bash

set -v

# test_execs=`find ./test_* -perm /u=x` 
test_execs=`find ./test_* -type f -perm +ugo+x -print`

echo "Tests to run: $test_execs"
while read -r testsuite; do
    ./"$testsuite"
    if [ "$?" -ne 0 ]; then
       echo "Aborting. \"$testsuite\" had failing test(s)"
       exit 1
    fi
done <<< "$test_execs"

