#!/bin/bash
if [ $# -neq 2 ]; then
    echo "Please provide a run number and output location"
    exit 1
fi

RED='\033[0;31m'
NC='\033[0m' # No Color

runNum=$1
output=$2
echo "Processing DRS/Pixel data for run ${runNum} in location ${output}"





