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

eval ./FCFSSimpleRouterTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./FCFSSwitchArbiterTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./FCFSVcAllocatorTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./genericBufferTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./genericIrisInterfaceTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./genericRCTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./genericTopoCreatorTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./ringTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./RRSimpleRouterTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./RRSwitchArbiterTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./RRVcAllocatorTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./SFP_FCFSSimpleRouterTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./torusTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./VNetFCFSSimpleRouterTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi

eval ./mcpCacheTest $OUT
if [ $? -ne 0 ]; then FAIL=1; fi


exit $FAIL

