#!/bin/bash
if [ $# -ne 2 ]; then
    echo "Please provide a run number and output file"
    exit 1
fi

RED='\033[0;31m'
NC='\033[0m' # No Color

runNum=$1
output=$2
echo "Processing DRS/Pixel data for run ${runNum} in location ${output}"
echo

echo "Copying DRS .dat files"
FILES=$(<Run${runNum}.list)
for f in $FILES
do
    xrdcp root://cmseos.fnal.gov//store/user/cmstestbeam/ETL/MT6Section1Data/122017/OTSDAQ/CMSTiming/${f} .
done
ndat=$(ls *.dat | wc -l)
if [ $ndat -lt 1 ]
then 
    echo "No DRS .dat files available"
    exit 1
fi
cat $(ls -v *.dat) > Run${runNum}.dat
echo

echo "Copying tracking file"
xrdcp root://cmseos.fnal.gov//store/user/cmstestbeam/ETL/MT6Section1Data/122017/OTSDAQ/CMSTiming/Run${runNum}_CMSTiming_converted.root Run${runNum}.root
if [ ! -f Run${runNum}.root ]
then
    echo "No tracking ROOT file available"
    exit 1
fi
echo

echo "Copying trigger count file"
xrdcp root://cmseos.fnal.gov//store/user/cmstestbeam/ETL/MT6Section1Data/122017/OTSDAQ/NimPlus/TriggerCount__${runNum}.cnt Run${runNum}.cnt
if [ ! -f Run${runNum}.cnt ]
then
    echo "No trigger count file available"
    exit 1
fi
echo

echo "Copying trigger timing file"
xrdcp root://cmseos.fnal.gov//store/user/cmstestbeam/ETL/MT6Section1Data/122017/OTSDAQ/NimPlus/TriggerTimingRun${runNum}_station5.txt Run${runNum}.txt
if [ ! -f Run${runNum}.txt ]
then
    echo "No trigger timing file available"
    exit 1
fi
echo

tar -xf calib.tar
mv calib/* .

echo "Processing DRS data file"
./dat2rootPixels Run${runNum}.dat Run${runNum}.root Run${runNum}.cnt Run${runNum}.txt OUTPUT.root -1 --config=December2017_LGADOnly.config --run_config=Run${runNum}.config
echo

echo "Copying DRS data file"
xrdcp OUTPUT.root root://cmseos.fnal.gov/${output}


