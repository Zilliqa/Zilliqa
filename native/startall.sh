#!/bin/bash
# Find all directories containing "native"
set -e


function start_task {
  local taskname=$1
  echo "Starting task "$taskname"..."
  dirs=$(find . -type d -name "$taskname" )
  # Loop through each directory and run "start.sh"
  for dir in $dirs; do
    if [ -f "$dir/start.sh" ]; then
      echo "Running start.sh in $dir..."
      cd "$dir"
      chmod +x start.sh && ./start.sh
      cd -
    else
      echo "No start.sh found in $dir"
    fi
  done
  echo "Done"
}

function start_webserver {
  echo "Starting webserver..."
  cd rundirs
  python3 -m http.server &
  cd -
  echo "Done"
}

function start_localstack {
  echo "Starting localstack..."
  localstack start -d
  echo "Done"
}


echo "starting all tasks..."
result=$(start_localstack)
result=$(start_webserver)

#result=$(start_task "*native-lookup*")
#result=$(start_task "*native-dsguard*")
#result=$(start_task "*native-normal*")
#result=$(start_task "*native-seedpub*")
#result=$(start_task "*native-multiplier*")

echo "$result"

