CXX = mpic++ -g
CPPFLAGS += -DKERNEL_UTEST -DIRIS_TEST -DNO_MPI -DSTATS -I/usr/include/cppunit -I../../../../../ -I../../components -I../../interfaces -I../
CPPFLAGS_MESSENGER = -DIRIS_TEST -I/usr/include/cppunit -I../../../../kernel/src -I../../components -I../../interfaces -I../

LDFLAGS += -lcppunit -lgslcblas -lgsl

EXECS = backPressureTest backPressureTest2  backPressureTest3  genericIrisInterfaceComponentTest  topoCreatorTest  topoCreatorTest2  topoCreatorCompTest  singleFlitPacketTest  topoCreatorMassTest

VPATH = ../../../../../kernel ../../components ../../data_types ../../interfaces

KERNEL_OBJS = KERNEL-clock.o KERNEL-component.o KERNEL-manifold.o KERNEL-messenger.o KERNEL-scheduler.o KERNEL-stat_engine.o KERNEL-syncalg.o KERNEL-lookahead.o

IRIS_COMPONENTS_OBJS = IRIS-CrossBarSwitch.o  IRIS-genericBuffer.o  IRIS-genericRC.o  IRIS-genericSwitchArbiter.o  IRIS-genericVcAllocator.o  IRIS-simpleArbiter.o  IRIS-simpleRouter.o

IRIS_FLIT_OBJS = IRIS-flit.o 

IRIS_INTERFACES_OBJS = IRIS-mapping.o 

IRIS_LINKDATA_OBJS = IRIS-linkData.o

IRIS_OBJS = $(IRIS_COMPONENTS_OBJS) $(IRIS_FLIT_OBJS) $(IRIS_INTERFACES_OBJS) $(IRIS_LINKDATA_OBJS)

ALL: $(EXECS)

backPressureTest: backPressureTest.o $(IRIS_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

backPressureTest2: backPressureTest2.o $(IRIS_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

backPressureTest3: backPressureTest3.o $(IRIS_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

genericIrisInterfaceComponentTest: genericIrisInterfaceComponentTest.o $(IRIS_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

singleFlitPacketTest: singleFlitPacketTest.o $(IRIS_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

topoCreatorTest: topoCreatorTest.o $(IRIS_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

topoCreatorTest2: topoCreatorTest2.o $(IRIS_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

topoCreatorCompTest: topoCreatorCompTest.o $(IRIS_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

topoCreatorMassTest: topoCreatorMassTest.o $(IRIS_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

pktGenTest: main.o pktGenTest.o flit.o pktgen.o networkPacket.o manifold.o component.o link.o clock.o messenger.o twoNode.o ninterface.o genericBuffer.o genericVcAllocator.o genericSwitchArbiter.o simpleRouter.o genericRC.o linkData.o mreqPkt.o pktgensubclass.o
	$(CXX) -o$@ $(LDFLAGS) $^
	

KERNEL-%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o KERNEL-$*.o
	
IRIS-%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o IRIS-$*.o
	
%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o $*.o

-include $(wildcard dep/*.d)

.PHONY: clean
clean:
	rm -f $(EXECS) *.o DBG* *.tr
	rm -rf dep

