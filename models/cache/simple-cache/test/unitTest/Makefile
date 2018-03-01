CXX = mpic++
CPPFLAGS += -g -DSIMPLE_CACHE_UTEST -DKERNEL_UTEST -DNO_MPI -I/usr/include/cppunit -I../.. -I../../../../..
LDFLAGS += -lcppunit
EXECS = hash_setTest hash_tableTest

VPATH = ../.. ../../../../../kernel


# If the simple-cache directory already has an object, say hash_table.o, then the object is
# not built. This may be wrong. So we use different names for those objects. This
# way the object files in the simple-cache directory are not picked up.
SIMPLE_CACHE_OBJS = SIMPLE-CACHE-cache_req.o SIMPLE-CACHE-hash_table.o 
KERNEL_OBJS = KERNEL-component.o KERNEL-manifold.o KERNEL-clock.o KERNEL-scheduler.o KERNEL-stat_engine.o

ALL: $(EXECS)


hash_tableTest: hash_tableTest.o  $(SIMPLE_CACHE_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

hash_setTest: hash_setTest.o  $(SIMPLE_CACHE_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)



SIMPLE-CACHE-%.o: %.cpp
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o SIMPLE-CACHE-$*.o

KERNEL-%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o KERNEL-$*.o

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
