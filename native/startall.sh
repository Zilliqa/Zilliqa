#!/bin/bash
# Find all directories containing "native"
dirs=$(find . -type d -name "*native-lookup*")
# Loop through each directory and run "start.sh"
for dir in $dirs; do
  if [ -f "$dir/start.sh" ]; then
    echo "Running start.sh in $dir..."
    cd $dir
    chmod +x start.sh && ./start.sh
    cd -
  else
    echo "No start.sh found in $dir"
  fi
done
# Loop through each directory and run "start.sh"
dirs=$(find . -type d -name "*native-dsguard*")
for dir in $dirs; do
  if [ -f "$dir/start.sh" ]; then
    echo "Running start.sh in $dir..."
    cd $dir
    chmod +x start.sh && ./start.sh
    cd -
  else
    echo "No start.sh found in $dir"
  fi
done
dirs=$(find . -type d -name "*native-normal*")
# Loop through each directory and run "start.sh"
for dir in $dirs; do
  if [ -f "$dir/start.sh" ]; then
    echo "Running start.sh in $dir..."
    cd $dir
    chmod +x start.sh && ./start.sh
    cd -
  else
    echo "No start.sh found in $dir"
  fi
done


dirs=$(find . -type d -name "*native-seedpub*")
# Loop through each directory and run "start.sh"
for dir in $dirs; do
  if [ -f "$dir/start.sh" ]; then
    echo "Running start.sh in $dir..."
    cd $dir
    chmod +x start.sh && ./start.sh
    cd -
  else
    echo "No start.sh found in $dir"
  fi
done

dirs=$(find . -type d -name "*native-multiplier*")
# Loop through each directory and run "start.sh"
for dir in $dirs; do
  if [ -f "$dir/start.sh" ]; then
    cd $dir
    echo "Running start.sh in $dir..."
    chmod +x start.sh && ./start.sh
    cd -
  else
    echo "No start.sh found in $dir"
  fi
done
