rm -f log
touch log
LOG=`pwd`/log

cd code/simulator/smp/QsimLib

make >> $LOG

for benchmark in /home/jenkins/qsim-benchmarks/parsec-tar/*.tar
do
	echo === $benchmark ===
	mpirun -np 1 ./smp_llp ../config/conf2x2_torus_llp.cfg ../config/6.cfg /home/jenkins/qsim/state.4 $benchmark >> $LOG
done