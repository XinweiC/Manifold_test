#!/bin/sh

#=====================================================================
# This script runs all the test programs. It can be run both from the
# command-line or from a script. Calling the script with a command-line
# argument (which can be any value) would cause the script not to produce
# any outputs, which could be useful when calling from another script.
# Specifically:
# - sh run_all.sh      - outputs will be produced
# - sh run_all.sh 1    - no outputs.
#=====================================================================

if [ $# -ne 0 ]; then
    OUT="> /dev/null 2>&1"
else
    OUT=""
fi

FAIL=0

eval ./backPressureTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./backPressureTest2 $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./backPressureTest3 $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./genericIrisInterfaceComponentTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./singleFlitPacketTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./topoCreatorTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./topoCreatorTest2 $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./topoCreatorCompTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./topoCreatorMassTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi


exit $FAIL

