#!/bin/bash
ver=`grep "define FW_VERSION" ../src/main.cpp | awk '{print $3'} | sed s/\"//g`
echo "firmware :" $ver
cp ../.pio/build/d1_mini/firmware.bin firmware-v$ver.bin

