CXX = mpic++
CPPFLAGS += -g -DKERNEL_UTEST -DNO_MPI -I/usr/include/cppunit -I../.. -I../../../../cache -I../../../../..
LDFLAGS += -lcppunit
EXECS = simple_mcTest

VPATH = ../.. ../../../../cache/simple-cache ../../../../../kernel

# If the kernel directory already has an object, say clock.o, then the object is
# not built. This may be wrong. So we use different names for kernel objects. This
# way the objects in the kernel directory are not picked up.
KERNEL_OBJS = KERNEL-clock.o KERNEL-component.o KERNEL-manifold.o KERNEL-messenger.o KERNEL-scheduler.o KERNEL-stat_engine.o

# Do the same for simple-mc
SIMPLE_MC_OBJS = SIMPLEMC-simple_mc.o

# Do the same for simple-cache
SIMPLE_CACHE_OBJS = SimpleCache-cache_messages.o


ALL: $(EXECS)

simple_mcTest: simple_mcTest.o $(SIMPLE_MC_OBJS) $(SIMPLE_CACHE_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)



KERNEL-%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o KERNEL-$*.o

SIMPLEMC-%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o SIMPLEMC-$*.o

SimpleCache-%.o: %.cpp
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o SimpleCache-$*.o

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
