CXX = mpic++ -g -Wall
CPPFLAGS += -DKERNEL_UTEST -DIRIS_TEST -DNO_MPI -I/usr/include/cppunit -I../../../../../ -I../../components -I../../interfaces -I../
CPPFLAGS_MESSENGER = -DIRIS_TEST -I/usr/include/cppunit -I../../../../kernel/src -I../../components -I../../interfaces -I../

LDFLAGS += -lcppunit -lgslcblas -lgsl

EXECS = genericBufferTest  genericIrisInterfaceTest  genericRCTest  FCFSSimpleRouterTest FCFSSwitchArbiterTest  FCFSVcAllocatorTest  genericTopoCreatorTest  ringTest  RRSimpleRouterTest  RRSwitchArbiterTest  RRVcAllocatorTest  SFP_FCFSSimpleRouterTest  torusTest  VNetFCFSSimpleRouterTest  mcpCacheTest


VPATH = ../../../../../kernel ../../components ../../data_types ../../interfaces

KERNEL_OBJS = KERNEL-clock.o KERNEL-component.o KERNEL-manifold.o KERNEL-messenger.o KERNEL-scheduler.o KERNEL-stat_engine.o  KERNEL-syncalg.o  KERNEL-lookahead.o

IRIS_COMPONENTS_OBJS = IRIS-genericBuffer.o  IRIS-genericRC.o  IRIS-genericSwitchArbiter.o  IRIS-genericVcAllocator.o  IRIS-simpleArbiter.o  IRIS-simpleRouter.o

IRIS_FLIT_OBJS = IRIS-flit.o 

IRIS_INTERFACES_OBJS = IRIS-mapping.o 

IRIS_LINKDATA_OBJS = IRIS-linkData.o

IRIS_OBJS = $(IRIS_COMPONENTS_OBJS) $(IRIS_FLIT_OBJS) $(IRIS_INTERFACES_OBJS) $(IRIS_LINKDATA_OBJS)

ALL: $(EXECS)

genericBufferTest: genericBufferTest.o IRIS-genericBuffer.o IRIS-flit.o
	$(CXX) -o$@ $^ $(LDFLAGS)

genericIrisInterfaceTest: genericIrisInterfaceTest.o $(IRIS_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

genericRCTest: genericRCTest.o IRIS-genericRC.o IRIS-flit.o
	$(CXX) -o$@ $^ $(LDFLAGS)

genOneVcIrisInterfaceTest: genOneVcIrisInterfaceTest.o $(IRIS_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

FCFSSimpleRouterTest: FCFSSimpleRouterTest.o $(KERNEL_OBJS) $(IRIS_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

FCFSSwitchArbiterTest: FCFSSwitchArbiterTest.o  IRIS-genericSwitchArbiter.o
	$(CXX) -o$@ $^ $(LDFLAGS)

FCFSVcAllocatorTest: FCFSVcAllocatorTest.o $(KERNEL_OBJS) $(IRIS_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

genericTopoCreatorTest: genericTopoCreatorTest.o $(KERNEL_OBJS) $(IRIS_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

	
ringTest: ringTest.o $(KERNEL_OBJS) $(IRIS_OBJS)  
	$(CXX) -o$@ $^ $(LDFLAGS)

RRSimpleRouterTest: RRSimpleRouterTest.o $(KERNEL_OBJS) $(IRIS_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

RRSwitchArbiterTest: RRSwitchArbiterTest.o  IRIS-genericSwitchArbiter.o
	$(CXX) -o$@ $^ $(LDFLAGS)

RRVcAllocatorTest: RRVcAllocatorTest.o $(KERNEL_OBJS) $(IRIS_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

SFP_FCFSSimpleRouterTest: SFP_FCFSSimpleRouterTest.o $(KERNEL_OBJS) $(IRIS_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

torusTest: torusTest.o $(KERNEL_OBJS) $(IRIS_OBJS)  
	$(CXX) -o$@ $^ $(LDFLAGS)

VNetFCFSSimpleRouterTest: VNetFCFSSimpleRouterTest.o $(KERNEL_OBJS) $(IRIS_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)


mcpCacheTest: mcpCacheTest.o $(KERNEL_OBJS) $(IRIS_OBJS)  
	$(CXX) -o$@ $^ $(LDFLAGS)

mcpCacheTest.o: mcpCacheTest.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) -I../../../../cache $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o $*.o


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

