#!/bin/bash

success=0
while [[ "${success}" == "0" ]]
do
   export TESTSEED=$RANDOM;
   run=0
   while [[ "${success}" == 0 && "${run}" -lt 5 ]]
   do
     pytest -s -v --log-level=DEBUG
     success="$?"
     run=$((run+1))
   done
done

