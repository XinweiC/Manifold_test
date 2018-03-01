#!/bin/sh

#=====================================================================
# This script runs all the test programs. It can be run both from the
# command-line or from a script. Calling the script with a command-line
# argument (which can be value) would cause the script not to produce
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

PROGRAMS="CmbTest1 CmbTest2 CmbTest3 CmbTest4 LBTSTest1 LBTSTest2 LBTSTest3 MessagingClockTest MessagingClockTest_ser MessagingClockTest_ser_pointer MessagingHalfClockTest MessagingHalfClockTest_ser MessagingHalfClockTest_ser_pointer MessagingHalfTest MessagingHalfTest_ser MessagingHalfTest_ser_pointer MessagingTest MessagingTest_ser MessagingTest_ser_pointer MessagingTest_ser_pointer2 MessagingTimeTest MessagingTimeTest_ser MessagingTimeTest_ser_pointer"

for p in $PROGRAMS; do
    eval mpirun -np 2 ./$p $OUT
    if [ $? -ne 0 ]; then FAIL=1; fi
done


PROGRAMS2="CmbTest1 CmbTest2 CmbTest3 CmbTest4 ManifoldConnectTest2 SchedulerTestTerminate SchedulerTestTerminate2"

for p in $PROGRAMS2; do
    eval mpirun -np 20 ./$p $OUT
    if [ $? -ne 0 ]; then FAIL=1; fi
done

eval mpirun -np 19 ./QtmTest4 10 $OUT
if [ $? -ne 0 ]; then FAIL=1; fi


PROGRAMS3="ManifoldConnectTest"

for p in $PROGRAMS3; do
    eval mpirun -np 5 ./$p $OUT
    if [ $? -ne 0 ]; then FAIL=1; fi
done

exit $FAIL
