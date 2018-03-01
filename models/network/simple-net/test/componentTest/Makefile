CXX = mpic++
CPPFLAGS += -g -Wall -DSIMPLE_NET_UTEST -DKERNEL_UTEST -DNO_MPI -I/usr/include/cppunit -I../.. -I../../../../../
LDFLAGS += -lcppunit
EXECS = meshNetworkTest ringNetworkTest

VPATH = ../.. ../../../../../kernel

# If the kernel directory already has an object, say clock.o, then the object is
# not built. This may be wrong. So we use different names for kernel objects. This
# way the objects in the kernel directory are not picked up.
KERNEL_OBJS = KERNEL-clock.o KERNEL-component.o KERNEL-manifold.o KERNEL-messenger.o KERNEL-scheduler.o KERNEL-stat_engine.o

# Do the same for simple-net
SIMPLE_NET_OBJS = SimpleNet-mapping.o SimpleNet-network.o SimpleNet-router.o


ALL: $(EXECS)

meshNetworkTest: meshNetworkTest.o $(SIMPLE_NET_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

ringNetworkTest: ringNetworkTest.o $(SIMPLE_NET_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)



KERNEL-%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o KERNEL-$*.o

SimpleNet-%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o SimpleNet-$*.o

%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o $*.o

%.o: %.cpp
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o $*.o

-include $(wildcard dep/*.d)

.PHONY: clean
clean:
	rm -f $(EXECS) *.o DBG*
	rm -rf dep
