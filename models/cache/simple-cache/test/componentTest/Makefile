CXX = mpic++
CPPFLAGS += -g -DSIMPLE_CACHE_UTEST -DKERNEL_UTEST -DNO_MPI -I/usr/include/cppunit -I../.. -I../../../../..
LDFLAGS += -lcppunit
EXECS = simple_cacheTest  simple_cacheTest2

VPATH = ../.. ../../../../../kernel

# If the kernel directory already has an object, say clock.o, then the object is
# not built. This may be wrong. So we use different names for kernel objects. This
# way the objects in the kernel directory are not picked up.
KERNEL_OBJS = KERNEL-clock.o KERNEL-component.o KERNEL-manifold.o KERNEL-messenger.o KERNEL-scheduler.o KERNEL-stat_engine.o  KERNEL-syncalg.o  KERNEL-lookahead.o

# Do the same for simple-cache
SIMPLE_CACHE_OBJS = SIMPLE-CACHE-simple_cache.o SIMPLE-CACHE-cache_req.o SIMPLE-CACHE-hash_table.o


ALL: $(EXECS)


simple_cacheTest: simple_cacheTest.o $(SIMPLE_CACHE_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

simple_cacheTest2: simple_cacheTest2.o $(SIMPLE_CACHE_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)



KERNEL-%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o KERNEL-$*.o

SIMPLE-CACHE-%.o: %.cpp
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o SIMPLE-CACHE-$*.o

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
