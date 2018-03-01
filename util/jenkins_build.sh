#!/bin/sh

RETURN_CODE=0

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
	
    RETURN_CODE=1
}

touch log
LOG=`pwd`/log
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
	error "ERROR: autoreconf failed"
    else
	./configure >> $LOG 2>&1
	if [ $? -ne 0 ]; then
	    error "ERROR: configure failed"
	else
	    make >> $LOG 2>&1
	    if [ $? -ne 0 ]; then
		error "ERROR: make failed" 
	    fi
	fi
    fi

    if [ -f ./lib$1.a ]; then
	echo "... lib$1.a successfully built." 
    else
	error "ERROR: lib$1.a NOT built."
    fi

    # run unit tests
    print_banner "Run $1 unit tests" >> $LOG
    echo cd $CODE_DIR/$2/test/unitTest >> $LOG
    cd $CODE_DIR/$2/test/unitTest

    make >> $LOG 2>&1
    sh run_all.sh >> $LOG 2>&1

    if [ $? -eq 0 ]; then
	echo "... $1 unit tests passed." 
    else
	error "ERROR: $1 unit tests failed."
    fi

    make clean >> $LOG 2>&1


    # run component tests
    print_banner "Run $1 component tests" >> $LOG
    echo cd $CODE_DIR/$2/test/componentTest >> $LOG
    cd $CODE_DIR/$2/test/componentTest

    make >> $LOG 2>&1
    sh run_all.sh >> $LOG 2>&1

    if [ $? -eq 0 ]; then
	echo "... $1 component tests passed." 
    else
	error "ERROR: $1 component tests failed."
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
# CaffDRAM
#===============================================================
component_test "caffdram" "models/memory/CaffDRAM"

#===============================================================
# MCP cache
#===============================================================
component_test "mcp-cache" "models/cache/mcp-cache"

#===============================================================
# iris
#===============================================================
component_test "iris" "models/network/iris"

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
    error "ERROR: autoreconf failed" 
else
    ./configure >> $LOG 2>&1
    if [ $? -ne 0 ]; then
	error "ERROR: configure failed"
    fi
fi


# do distcheck
print_banner "Run make distcheck" >> $LOG

make distcheck >> $LOG 2>&1

if [ $? -eq 0 ]; then
    echo "... distcheck passed." 
else
    error "ERROR: distcheck failed." 
fi

# final clean up
cd $CODE_DIR
make maintainer-clean >> $LOG 2>&1

exit $RETURN_CODE


