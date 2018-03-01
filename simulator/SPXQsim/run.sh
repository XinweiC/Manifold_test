#!/bin/bash

TESTBENCH=`ls ./benchmarks3/`

trap 'exit 1' 1 2 15

#export LD_LIBRARY_PATH=$QSIM_PREFIX/lib
export QSIM_PREFIX=/home/hugh/Project/HARDWARE/ME/local_install/qsim-root
export LD_LIBRARY_PATH=/home/hugh/Project/HARDWARE/ME/local_install/qsim-root/lib

mkdir -p results

for testcase in $TESTBENCH; do
  echo $testcase
  ./smp_llp_con_dvfs conf_llp.cfg.bst.ss outorder.config.ss qsim_lib state.16 16core_ooo.config.ss ./benchmarks/$testcase >./results/$testcase.best.log 2>&1
done
