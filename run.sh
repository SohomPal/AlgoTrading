#!/bin/bash

set -e

EXECUTABLE="./build/AlgoTrader"

if [ ! -f "$EXECUTABLE" ]; then
  echo "Error: Executable not found at $EXECUTABLE"
  echo "Run ./build.sh first to build the project."
  exit 1
fi

echo "Running Executable"
$EXECUTABLE > output.log
