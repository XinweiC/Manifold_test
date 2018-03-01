CC = g++

##################################################################

MAKE = make
AR = ar qcv
RANLIB = ranlib
RM = rm -rf
LN = ln -s
LNDIR = ln -s
DIFF = diff
OEXT = o
LEXT = a
EEXT =
CFLAGS = -g

##################################################################

SRCS = \
parameters.cc \
parser.cc \
energy_introspector.cc

HDRS = \
parameters.h \
parser.h \
queue.h \
library.h \
energy_introspector.h

OBJS = \
parameters.$(OEXT) \
parser.$(OEXT) \
energy_introspector.$(OEXT)

#ifneq ($(wildcard ENERGYLIB_DRAMSIM/dramninjasUMD-DRAMSim2-82aa601/libdramsim.so),)
#	ENERGYLIB_DRAMSIM_LIB += -ldramsim
#	ENERGYLIB_DRAMSIM_FLAGS += -IENERGYLIB_DRAMSIM/dramninjasUMD-DRAMSim2-82aa601 -LENERGYLIB_DRAMSIM/dramninjasUMD-DRAMSim2-82aa601
#	ENERGYLIB_DRAMSIM_OBJS += ENERGYLIB_DRAMSIM.o
#	LIBRARY_LIBS += $(ENERGYLIB_DRAMSIM_LIBS)
#	LIBRARY_FLAGS += $(ENERGYLIB_DRAMSIM_FLAGS)
#	LIBRARY_OBJS += $(ENERGYLIB_DRAMSIM_OBJS)
#endif

ifneq ($(wildcard ENERGYLIB_DSENT/dsent0.91/libdsent.a),)
	ENERGYLIB_DSENT_LIBS += -ldsent -lutil
	ENERGYLIB_DESENT_FLAGS += -IENERGYLIB_DSENT/dsent0.91 -IENERGYLIB_DSENT/dsent0.91/model -IENERGYLIB_DSENT/dsent0.91/libutil -LENERGYLIB_DSENT/dsent0.91 -LENERGYLIB_DSENT/dsent0.91/libutil
	ENERGYLIB_DSENT_OBJS += ENERGYLIB_DSENT.o
	LIBRARY_LIBS += $(ENERGYLIB_DSENT_LIBS)
	LIBRARY_FLAGS += $(ENERGYLIB_DSENT_FLAGS)
	LIBRARY_OBJS += $(ENERGYLIB_DSENT_OBJS)
endif

ifneq ($(wildcard ENERGYLIB_INTSIM/intsim_alpha/libintsim.a),)
	ENERGYLIB_INTSIM_LIBS += -lintsim
	ENERGYLIB_INTSIM_FLAGS += -IENERGYLIB_INTSIM/intsim_alpha -LENERGYLIB_INTSIM/intsim_alpha
	ENERGYLIB_INTSIM_OBJS += ENERGYLIB_INTSIM.o
	LIBRARY_LIBS += $(ENERGYLIB_INTSIM_LIBS)
	LIBRARY_FLAGS += $(ENERGYLIB_INTSIM_FLAGS)
	LIBRARY_OBJS += $(ENERGYLIB_INTSIM_OBJS)
endif

ifneq ($(wildcard ENERGYLIB_MCPAT/mcpat0.8_r274/libmcpat.a),)
	ENERGYLIB_MCPAT_LIBS += -lmcpat
	ENERGYLIB_MCPAT_FLAGS += -IENERGYLIB_MCPAT/mcpat0.8_r274 -IENERGYLIB_MCPAT/mcpat0.8_r274/cacti -LENERGYLIB_MCPAT/mcpat0.8_r274
	ENERGYLIB_MCPAT_OBJS += ENERGYLIB_MCPAT.o
	LIBRARY_LIBS += $(ENERGYLIB_MCPAT_LIBS)
	LIBRARY_FLAGS += $(ENERGYLIB_MCPAT_FLAGS)
	LIBRARY_OBJS += $(ENERGYLIB_MCPAT_OBJS)
endif

ifneq ($(wildcard ENERGYLIB_TSV/libtsv.a),)
	ENERGYLIB_TSV_LIBS += -ltsv
	ENERGYLIB_TSV_FLAGS += -IENERGYLIB_TSV -LENERGYLIB_TSV
	ENERGYLIB_TSV_OBJS += ENERGYLIB_TSV.o
	LIBRARY_LIBS += $(ENERGYLIB_TSV_LIBS)
	LIBRARY_FLAGS += $(ENERGYLIB_TSV_FLAGS)
	LIBRARY_OBJS += $(ENERGYLIB_TSV_OBJS)
endif

ifneq ($(wildcard RELIABILITYLIB_FAILURE/libfailure.a),)
	RELIABILITYLIB_FAILURE_LIBS += -lfailure
	RELIABILITYLIB_FAILURE_FLAGS += -IRELIABILITYLIB_FAILURE -LRELIABILITYLIB_FAILURE
	RELIABILITYLIB_FAILURE_OBJS += RELIABILITYLIB_FAILURE.o
	LIBRARY_LIBS += $(RELIABILITYLIB_FAILURE_LIBS)
	LIBRARY_FLAGS += $(RELIABILITYLIB_FAILURE_FLAGS)
	LIBRARY_OBJS += $(RELIABILITYLIB_FAILURE_OBJS)
endif

ifneq ($(wildcard SENSORLIB_RANDOM/librandom.a),)
	SENSORLIB_RANDOM_FLAGS += -ISENSORLIB_RANDOM -LSENSORLIB_RANDOM
	SENSORLIB_RANDOM_OBJS += SENSORLIB_RANDOM.o
	SENSORLIB_RANDOM_LIBS += -lrandom
	LIBRARY_LIBS += $(SENSORLIB_RANDOM_LIBS)
	LIBRARY_FLAGS += $(SENSORLIB_RANDOM_FLAGS)
	LIBRARY_OBJS += $(SENSORLIB_RANDOM_OBJS)
endif

ifneq ($(wildcard THERMALLIB_3DICE/3d-ice/lib/libthreed-ice-2.1.a),)
	THERMALLIB_3DICE_FLAGS += -ITHERMALLIB_3DICE/3d-ice/sources -ITHERMALLIB_3DICE/3d-ice/bison -ITHERMALLIB_3DICE/3d-ice/flex -ITHERMALLIB_3DICE/SuperLU_4.3/SRC -LTHERMALLIB_3DICE/3d-ice/lib -LTHERMALLIB_3DICE/SuperLU_4.3/lib
	THERMALLIB_3DICE_OBJS += THERMALLIB_3DICE.o
	THERMALLIB_3DICE_LIBS += -lthreed-ice-2.1 -lsuperlu_4.3 -lblas
	LIBRARY_LIBS += $(THERMALLIB_3DICE_LIBS)
	LIBRARY_FLAGS += $(THERMALLIB_3DICE_FLAGS)
	LIBRARY_OBJS += $(THERMALLIB_3DICE_OBJS)
endif

ifneq ($(wildcard THERMALLIB_HOTSPOT/HotSpot-5.02/libhotspot.a),)
	THERMALLIB_HOTSPOT_FLAGS += -ITHERMALLIB_HOTSPOT/HotSpot-5.02 -LTHERMALLIB_HOTSPOT/HotSpot-5.02
	THERMALLIB_HOTSPOT_OBJS += THERMALLIB_HOTSPOT.o
	THERMALLIB_HOTSPOT_LIBS += -lhotspot
	LIBRARY_LIBS += $(THERMALLIB_HOTSPOT_LIBS)
	LIBRARY_FLAGS += $(THERMALLIB_HOTSPOT_FLAGS)
	LIBRARY_OBJS += $(THERMALLIB_HOTSPOT_OBJS)
endif

default:
	@echo "make lib; or make test;"

includelib: 
	$(RM) $(OBJS)
	$(RM) include.h
	touch include.h
ifneq ($(wildcard ENERGYLIB_DSENT/dsent0.91/libdsent.a),)
	@echo "#define ENERGYLIB_DSENT_I" >> include.h
endif
ifneq ($(wildcard ENERGYLIB_INTSIM/intsim_alpha/libintsim.a),)
	@echo "#define ENERGYLIB_INTSIM_I" >> include.h
endif
ifneq ($(wildcard ENERGYLIB_MCPAT/mcpat0.8_r274/libmcpat.a),)
	@echo "#define ENERGYLIB_MCPAT_I" >> include.h
endif
ifneq ($(wildcard ENERGYLIB_TSV/libtsv.a),)
	@echo "#define ENERGYLIB_TSV_I" >> include.h
endif
ifneq ($(wildcard RELIABILITYLIB_FAILURE/libfailure.a),)
	@echo "#define RELIABILITYLIB_FAILURE_I" >> include.h
endif
ifneq ($(wildcard SENSORLIB_RANDOM/librandom.a),)
	@echo "#define SENSORLIB_RANDOM_I" >> include.h
endif
ifneq ($(wildcard THERMALLIB_3DICE/3d-ice/lib/libthreed-ice-2.1.a),)
	@echo "#define THERMALLIB_3DICE_I" >> include.h
endif
ifneq ($(wildcard THERMALLIB_HOTSPOT/HotSpot-5.02/libhotspot.a),)
	@echo "#define THERMALLIB_HOTSPOT_I" >> include.h
endif

#test$(EEXT): includelib $(LIBRARY_OBJS) $(OBJS) test.$(OEXT)
#	$(CC) -o test$(EEXT) -pthread $(CFLAGS) $(LIBRARY_FLAGS) $(LIBRARY_OBJS) $(OBJS) test.$(OEXT) $(LIBRARY_LIBS)
#ifneq ($(ENERGYLIB_INTSIM_OBJS),)
#	make test-intsim
#endif

test: includelib $(LIBRARY_OBJS) $(OBJS)
	@echo $(LIBRARY_OBJS)
ifneq ($(ENERGYLIB_DESENT_OBJS),)
	$(RM) test-dsent
	make test-dsent
endif
ifneq ($(ENERGYLIB_INTSIM_OBJS),)
	$(RM) test-intsim
	make test-intsim
endif
ifneq ($(ENERGYLIB_MCPAT_OBJS),)
	$(RM) test-mcpat
	make test-mcpat
endif
ifneq ($(ENERGYLIB_TSV_OBJS),)
	$(RM) test-tsv
	make test-tsv
endif
ifneq ($(RELIABILITYLIB_FAILURE_OBJS),)
	$(RM) test-failure
	make test-failure
endif
ifneq ($(SENSORLIB_RANDOM_OBJS),)
	$(RM) test-random
	make test-random
endif
ifneq ($(THERMALLIB_3DICE_OBJS),)
	$(RM) test-3dice
	make test-3dice
endif
ifneq ($(THERMALLIB_HOTSPOT_OBJS),)
	$(RM) test-hotspot
	make test-hotspot
endif

test-dsent$(EEXT): TEST_MAIN/test-dsent.$(OEXT)
	$(CC) -o test-dsent$(EEXT) -pthread $(CFLAGS) $(LIBRARY_FLAGS) $(LIBRARY_OBJS) $(OBJS) TEST_MAIN/test-dsent.$(OEXT) $(LIBRARY_LIBS)

test-intsim$(EEXT): TEST_MAIN/test-intsim.$(OEXT)
	$(CC) -o test-intsim$(EEXT) -pthread $(CFLAGS) $(LIBRARY_FLAGS) $(LIBRARY_OBJS) $(OBJS) TEST_MAIN/test-intsim.$(OEXT) $(LIBRARY_LIBS)

test-mcpat$(EEXT): TEST_MAIN/test-mcpat.$(OEXT)
	$(CC) -o test-mcpat$(EEXT) -pthread $(CFLAGS) $(LIBRARY_FLAGS) $(LIBRARY_OBJS) $(OBJS) TEST_MAIN/test-mcpat.$(OEXT) $(LIBRARY_LIBS)

test-tsv$(EEXT): TEST_MAIN/test-tsv.$(OEXT)
	$(CC) -o test-tsv$(EEXT) -pthread $(CFLAGS) $(LIBRARY_FLAGS) $(LIBRARY_OBJS) $(OBJS) TEST_MAIN/test-tsv.$(OEXT) $(LIBRARY_LIBS)

test-failure$(EEXT): TEST_MAIN/test-failure.$(OEXT)
	$(CC) -o test-failure$(EEXT) -pthread $(CFLAGS) $(LIBRARY_FLAGS) $(LIBRARY_OBJS) $(OBJS) TEST_MAIN/test-failure.$(OEXT) $(LIBRARY_LIBS)

test-random$(EEXT): TEST_MAIN/test-random.$(OEXT)
	$(CC) -o test-random$(EEXT) -pthread $(CFLAGS) $(LIBRARY_FLAGS) $(LIBRARY_OBJS) $(OBJS) TEST_MAIN/test-random.$(OEXT) $(LIBRARY_LIBS)

test-3dice$(EEXT): TEST_MAIN/test-3dice.$(OEXT)
	$(CC) -o test-3dice$(EEXT) -pthread $(CFLAGS) $(LIBRARY_FLAGS) $(LIBRARY_OBJS) $(OBJS) TEST_MAIN/test-3dice.$(OEXT) $(LIBRARY_LIBS)

test-hotspot$(EEXT): TEST_MAIN/test-hotspot.$(OEXT)
	$(CC) -o test-hotspot$(EEXT) -pthread $(CFLAGS) $(LIBRARY_FLAGS) $(LIBRARY_OBJS) $(OBJS) TEST_MAIN/test-hotspot.$(OEXT) $(LIBRARY_LIBS)

parameters.$(OEXT):
	$(CC) $(CFLAGS) -o parameters.$(OEXT) -c parameters.cc

parser.$(OEXT):
	$(CC) $(CFLAGS) -o parser.$(OEXT) -c parser.cc
	
energy_introspector.$(OEXT):
	$(CC) $(CFLAGS) $(LIBRARY_FLAGS) -o energy_introspector.$(OEXT) -c energy_introspector.cc

#test.$(OEXT):
#	$(CC) $(CFLAGS) $(LIBRARY_FLAGS) -o test.$(OEXT) -c test.cc

ENERGYLIB_DSENT.$(OEXT):
	$(CC) $(CFLAGS) $(ENERGYLIB_DSENT_FLAGS) -o ENERGYLIB_DSENT.$(OEXT) -c ENERGYLIB_DSENT.cc

ENERGYLIB_INTSIM.$(OEXT):
	$(CC) $(CFLAGS) $(ENERGYLIB_INTSIM_FLAGS) -o ENERGYLIB_INTSIM.$(OEXT) -c ENERGYLIB_INTSIM.cc

ENERGYLIB_MCPAT.$(OEXT):
	$(CC) $(CFLAGS) $(ENERGYLIB_MCPAT_FLAGS) -o ENERGYLIB_MCPAT.$(OEXT) -c ENERGYLIB_MCPAT.cc

ENERGYLIB_TSV.$(OEXT):
	$(CC) $(CFLAGS) $(ENERGYLIB_TSV_FLAGS) -o ENERGYLIB_TSV.$(OEXT) -c ENERGYLIB_TSV.cc

RELIABILITYLIB_FAILURE.$(OEXT):
	$(CC) $(CFLAGS) $(RELIABILITYLIB_FAILURE_FLAGS) -o RELIABILITYLIB_FAILURE.$(OEXT) -c RELIABILITYLIB_FAILURE.cc

SENSORLIB_RANDOM.$(OEXT):
	$(CC) $(CFLAGS) $(SENSORLIB_RANDOM_FLAGS) -o SENSORLIB_RANDOM.$(OEXT) -c SENSORLIB_RANDOM.cc

THERMALLIB_3DICE.$(OEXT):
	$(CC) $(CFLAGS) $(THERMALLIB_3DICE_FLAGS) -o THERMALLIB_3DICE.$(OEXT) -c THERMALLIB_3DICE.cc
	
THERMALLIB_HOTSPOT.$(OEXT):
	$(CC) $(CFLAGS) $(THERMALLIB_HOTSPOT_FLAGS) -o THERMALLIB_HOTSPOT.$(OEXT) -c THERMALLIB_HOTSPOT.cc

#.cc.$(OEXT):
#	$(CC) $(CFLAGS) $(LIBRARY_FLAGS) -o $*.$(OEXT) -c $*.cc
	
libEI: includelib $(LIBRARY_OBJS) $(OBJS)
	ar qcv libEI.a $(LIBRARY_OBJS) $(OBJS)
	ranlib libEI.a

intsim:
ifneq ($(wildcard ENERGYLIB_INTSIM/intsim_alpha),)
	cd ENERGYLIB_INTSIM/intsim_alpha/; make clean
	cd ENERGYLIB_INTSIM/intsim_alpha/; make lib
else
	@echo "ERROR: ENERGYLIB_INTSIM/intsim_alpha/ directory not found"
endif

dsent:
ifneq ($(wildcard ENERGYLIB_DSENT/dsent0.91),)
	cd ENERGYLIB_DSENT/dsent0.91/; cp ../patch/Makefile ./
	cd ENERGYLIB_DSENT/dsent0.91/; make clean
	cd ENERGYLIB_DSENT/dsent0.91/; make lib
else
	@echo "ERROR: ENERGYLIB_DSENT/dsent0.91/ directory not found"
endif

mcpat:
ifneq ($(wildcard ENERGYLIB_MCPAT/mcpat0.8_r274),)
	cd ENERGYLIB_MCPAT/mcpat0.8_r274/; make clean
	cd ENERGYLIB_MCPAT/mcpat0.8_r274/; cp ../patch/makefile ./; cp ../patch/mcpat.mk ./
	cd ENERGYLIB_MCPAT/mcpat0.8_r274/; cp ../patch/*.cc ./; cp ../patch/*.h ./
	cd ENERGYLIB_MCPAT/mcpat0.8_r274/cacti/; cp ../../patch/cacti/*.cc ./; cp ../../patch/cacti/*.h ./
	cd ENERGYLIB_MCPAT/mcpat0.8_r274/; make lib
else
	@echo "ERROR: ENERGYLIB_MCPAT/mcpat0.8_r274/ directory not found"
endif

tsv:
ifneq ($(wildcard ENERGYLIB_TSV),)
	cd ENERGYLIB_TSV/; make clean
	cd ENERGYLIB_TSV/; make lib
else
	@echo "ERROR: ENERGYLIB_TSV/ directory not found"
endif

random:
ifneq ($(wildcard SENSORLIB_RANDOM),)
	cd SENSORLIB_RANDOM/; make clean
	cd SENSORLIB_RANDOM/; make lib
else
	@echo "ERROR: SENSORLIB_RANDOM/ directory not found"
endif

failure:
ifneq ($(wildcard RELIABILITYLIB_FAILURE),)
	cd RELIABILITYLIB_FAILURE/; make clean
	cd RELIABILITYLIB_FAILURE/; make lib
else
	@echo "ERROR: RELIABILITYLIB_FAILURE/ directory not found"
endif

3dice: 
ifneq ($(wildcard THERMALLIB_3DICE/SuperLU_4.3),)
ifneq ($(wildcard THERMALLIB_3DICE/3d-ice),)
	cd THERMALLIB_3DICE/SuperLU_4.3/; cp ../patch/make.inc ./;
	cd THERMALLIB_3DICE/SuperLU_4.3/; make clean
	cd THERMALLIB_3DICE/SuperLU_4.3/; make blaslib; make superlulib
	cd THERMALLIB_3DICE/3d-ice/; cp ../patch/makefile.def ./; cp ../patch/*.h include/; cp ../patch/*.c sources/;
	cd THERMALLIB_3DICE/3d-ice/; make clean
	cd THERMALLIB_3DICE/3d-ice/; make lib
else
	@echo "ERROR: THERMALLIB_3DICE/3d-ice/ directory not found"
endif 
else
	@echo "ERROR: THERMALLIB_3DICE/SuperLU_4.3/ directory not found"
endif

hotspot:
ifneq ($(wildcard THERMALLIB_HOTSPOT),)
	cd THERMALLIB_HOTSPOT/HotSpot-5.02/; cp ../patch/* ./;
	cd THERMALLIB_HOTSPOT/HotSpot-5.02/; make clean
	cd THERMALLIB_HOTSPOT/HotSpot-5.02/; make lib
else
	@echo "ERROR: THERMALLIB_HOTSPOT/HotSpot-5.02/ directory not found"
endif

all: intsim mcpat tsv random failure 3dice hotspot

filelist:
	@echo $(SRCS) $(HDRS) makefile

clean:
	$(RM) libEI.a test-* TEST_MAIN/*.o *.out *.o *~
	cd CONFIG/; $(RM) *.out *~
	cd CONFIG/INTSIM/; $(RM) *.out *~
	cd CONFIG/DSENT/; $(RM) *.out *~
	cd CONFIG/MCPAT/; $(RM) *.out *~
	cd CONFIG/TSV/; $(RM) *.out *~
	cd CONFIG/RANDOM/; $(RM) *.out *~
	cd CONFIG/FAILURE/; $(RM) *.out *~
	cd CONFIG/3DICE/; $(RM) *.out *~
	cd CONFIG/HOTSPOT/; $(RM) *.out *~

cleanall:
ifneq ($(wildcard ENERGYLIB_INTSIM/intsim_alpha),)
	cd ENERGYLIB_INTSIM/intsim_alpha/; make clean
endif
ifneq ($(wildcard ENERGYLIB_DSENT/dsent0.91),)
	cd ENERGYLIB_DSENT/dsent0.91/; make clean
endif
ifneq ($(wildcard ENERGYLIB_MCPAT/mcpat0.8_r274),)
	cd ENERGYLIB_MCPAT/mcpat0.8_r274/; make clean
endif
ifneq ($(wildcard ENERGYLIB_TSV),)
	cd ENERGYLIB_TSV/; make clean
endif
ifneq ($(wildcard SENSORLIB_RANDOM),)
	cd SENSORLIB_RANDOM; make clean
endif
ifneq ($(wildcard RELIABILITYLIB_FAILURE),)
	cd RELIABILITYLIB_FAILURE/; make clean
endif
ifneq ($(wildcard THERMALLIB_3DICE/SuperLU_4.3),)
	cd THERMALLIB_3DICE/SuperLU_4.3/; make clean
endif
ifneq ($(wildcard THERMALLIB_3DICE/3d-ice),)
	cd THERMALLIB_3DICE/3d-ice/; make clean
endif
ifneq ($(wildcard THERMALLIB_HOTSPOT/HotSpot-5.02),)
	cd THERMALLIB_HOTSPOT/HotSpot-5.02/; make clean
endif
	$(RM) libEI.a test-* TEST_MAIN/*.o *.out *.o *~
	cd CONFIG/; $(RM) *.out *~
	cd CONFIG/INTSIM/; $(RM) *.out *~
	cd CONFIG/DSENT/; $(RM) *.out *~
	cd CONFIG/MCPAT/; $(RM) *.out *~
	cd CONFIG/TSV/; $(RM) *.out *~
	cd CONFIG/RANDOM/; $(RM) *.out *~
	cd CONFIG/FAILURE/; $(RM) *.out *~
	cd CONFIG/3DICE/; $(RM) *.out *~
	cd CONFIG/HOTSPOT/; $(RM) *.out *~

