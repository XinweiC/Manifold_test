#!/bin/sh


BUILD_HOME=/home/jwang/MANIFOLD_daily

cd $BUILD_HOME

N=6  # up to N historical builds are kept

#remove oldest if limit has been reached
buildNs=`ls -d build${N}.* 2> /dev/null`
if [ -n "$buildNs" ] ; then
    chmod -R 777 build${N}.*
    rm -rf build${N}.*
fi

d=`expr $N - 1`

while [ $d -gt 0 ]; do
    files=`ls -d build$d.* 2> /dev/null`
    for f in $files; do  #there should only be 1 file, but just in case
        extension=`echo $f | cut -d. -f 2-`
	d1=`expr $d + 1`
	mv $f build${d1}.$extension
    done
    d=`expr $d - 1`
done

# special handling of build.something
files=`ls -d build.* 2> /dev/null`
for f in $files; do  #there should only be 1 file, but just in case
    extension=`echo $f | cut -d. -f 2-`
    mv $f build1.$extension
done




WORK_DIR=`pwd`/build.`date +%b%d`
mkdir -p $WORK_DIR

cd $WORK_DIR
touch log report
LOG=`pwd`/log
REPORT=`pwd`/report

print_banner() {
    echo
    echo "=================================================================="
    echo $1
    echo "=================================================================="
}

error() {
    echo
    echo "#########################"
    echo $1
    echo "#########################"
}

error_exit() {
    echo
    echo "#########################"
    echo $1
    echo "#########################"
    exit 1
}


echo "... Building Manifold, `date`" >> $LOG
echo >> $LOG

date >> $REPORT
echo >> $REPORT

svn co https://svn.ece.gatech.edu/repos/Manifold/trunk/code >> $LOG 2>&1
cd code

CODE_DIR=`pwd`


######################################################################
# First build / test each component individually
######################################################################


#===============================================================
# Define a function to test components. 
#===============================================================
# 1st parameter: name of the library
# 2nd parameter: pathname relative to code
component_test() {
    print_banner "... Building $1" >> $LOG
    cd $CODE_DIR/$2

    autoreconf -si >> $LOG 2>&1
    if [ $? -ne 0 ]; then
	echo "ERROR: autoreconf failed" >> $REPORT
    else
	./configure >> $LOG 2>&1
	if [ $? -ne 0 ]; then
	    echo "ERROR: configure failed" >> $REPORT
	else
	    make >> $LOG 2>&1
	    if [ $? -ne 0 ]; then
		echo "ERROR: make failed" >> $REPORT
	    fi
	fi
    fi

    if [ -f ./lib$1.a ]; then
	echo "... lib$1.a successfully built." >> $REPORT
    else
	echo "ERROR: lib$1.a NOT built." >> $REPORT
    fi

    # run unit tests
    print_banner "Run $1 unit tests" >> $LOG
    echo cd $CODE_DIR/$2/test/unitTest >> $LOG
    cd $CODE_DIR/$2/test/unitTest

    make >> $LOG 2>&1
    sh run_all.sh >> $LOG 2>&1

    if [ $? -eq 0 ]; then
	echo "... $1 unit tests passed." >> $REPORT
    else
	echo "ERROR: $1 unit tests failed." >> $REPORT
    fi

    make clean >> $LOG 2>&1


    # run component tests
    print_banner "Run $1 component tests" >> $LOG
    echo cd $CODE_DIR/$2/test/componentTest >> $LOG
    cd $CODE_DIR/$2/test/componentTest

    make >> $LOG 2>&1
    sh run_all.sh >> $LOG 2>&1

    if [ $? -eq 0 ]; then
	echo "... $1 component tests passed." >> $REPORT
    else
	echo "ERROR: $1 component tests failed." >> $REPORT
    fi

    make clean >> $LOG 2>&1


    # final clean up
    cd $CODE_DIR/$2
    make maintainer-clean >> $LOG 2>&1
} #component_test()



#===============================================================
# kernel
#===============================================================
component_test "manifold" "kernel"


#===============================================================
# simple-cache
#===============================================================
component_test "simple-cache" "models/cache/simple-cache"

#===============================================================
# CaffDRAM
#===============================================================
component_test "caffdram" "models/memory/CaffDRAM"

#===============================================================
# simple-mc
#===============================================================
component_test "simple-mc" "models/memory/simple-mc"

#===============================================================
# iris
#===============================================================
component_test "iris" "models/network/iris"

#===============================================================
# simple-net
#===============================================================
component_test "simple-net" "models/network/simple-net"

#===============================================================
# simple-proc
#===============================================================
component_test "simple-proc" "models/processor/simple-proc"

#===============================================================
# zesto
#===============================================================
component_test "Zesto" "models/processor/zesto"



######################################################################
# distcheck for all
######################################################################
print_banner "... Distcheck for all" >> $LOG
cd $CODE_DIR

autoreconf -si >> $LOG 2>&1
if [ $? -ne 0 ]; then
    echo "ERROR: autoreconf failed" >> $REPORT
else
    ./configure >> $LOG 2>&1
    if [ $? -ne 0 ]; then
	echo "ERROR: configure failed" >> $REPORT
    fi
fi


# do distcheck
print_banner "Run make distcheck" >> $LOG

make distcheck >> $LOG 2>&1

if [ $? -eq 0 ]; then
    echo "... distcheck passed." >> $REPORT
else
    echo "ERROR: distcheck failed." >> $REPORT
fi

#rm -f manifold_kernel*.tar.gz

# final clean up
cd $CODE_DIR
make maintainer-clean >> $LOG 2>&1







