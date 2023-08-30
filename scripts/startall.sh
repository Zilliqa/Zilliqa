#!/bin/bash
# Find all directories containing "localdev"
dirs=$(find . -type d -name "*localdev-dsguard*")
# Loop through each directory and run "start.sh"
for dir in $dirs; do
  if [ -f "$dir/start.sh" ]; then
    echo "Running start.sh in $dir..."
    cd $dir
    ./start.sh
    cd -
  else
    echo "No start.sh found in $dir"
  fi
done

dirs=$(find . -type d -name "*localdev-normal*")
# Loop through each directory and run "start.sh"
for dir in $dirs; do
  if [ -f "$dir/start.sh" ]; then
    echo "Running start.sh in $dir..."
    cd $dir
    ./start.sh
    cd -
  else
    echo "No start.sh found in $dir"
  fi
done

dirs=$(find . -type d -name "*localdev-lookup*")
# Loop through each directory and run "start.sh"
for dir in $dirs; do
  if [ -f "$dir/start.sh" ]; then
    echo "Running start.sh in $dir..."
    cd $dir
    ./start.sh
    cd -
  else
    echo "No start.sh found in $dir"
  fi
done

dirs=$(find . -type d -name "*localdev-seedpub*")
# Loop through each directory and run "start.sh"
for dir in $dirs; do
  if [ -f "$dir/start.sh" ]; then
    echo "Running start.sh in $dir..."
    cd $dir
    ./start.sh
    cd -
  else
    echo "No start.sh found in $dir"
  fi
done

dirs=$(find . -type d -name "*localdev-multiplier*")
# Loop through each directory and run "start.sh"
for dir in $dirs; do
  if [ -f "$dir/start.sh" ]; then
    cd $dir
    echo "Running start.sh in $dir..."
    ./start.sh
    cd -
  else
    echo "No start.sh found in $dir"
  fi
done
