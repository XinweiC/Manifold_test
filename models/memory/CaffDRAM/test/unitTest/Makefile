CXX = mpic++  #-fprofile-arcs -ftest-coverage
CPPFLAGS += -g -Wall -DCAFFDRAM_TEST -DKERNEL_UTEST -DNO_MPI -I/usr/include/cppunit -I../.. -I../../../../../ -I../../../../cache
LDFLAGS += -lcppunit

EXECS = bankTest channelTest controllerTest dreqTest rankTest mcMapTest

VPATH = ../.. ../../../../../kernel

# If the kernel directory already has an object, say clock.o, then the object is
# not built. This may be wrong. So we use different names for kernel objects. This
# way the objects in the kernel directory are not picked up.
KERNEL_OBJS = KERNEL-clock.o KERNEL-component.o KERNEL-manifold.o KERNEL-messenger.o KERNEL-scheduler.o KERNEL-stat_engine.o KERNEL-syncalg.o KERNEL-lookahead.o

# Do the same for caffdram
CAFFDRAM_OBJS1 = CAFFDRAM-Bank.o CAFFDRAM-Channel.o CAFFDRAM-Dreq.o CAFFDRAM-Dsettings.o CAFFDRAM-Rank.o
CAFFDRAM_OBJS2 = CAFFDRAM-Controller.o $(CAFFDRAM_OBJS1)


ALL: $(EXECS)

bankTest: bankTest.o $(CAFFDRAM_OBJS1) KERNEL-stat_engine.o
	$(CXX) -o$@ $^ $(LDFLAGS)

channelTest: channelTest.o $(CAFFDRAM_OBJS1) KERNEL-stat_engine.o
	$(CXX) -o$@ $^ $(LDFLAGS)

controllerTest: controllerTest.o $(CAFFDRAM_OBJS2) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

dreqTest: dreqTest.o $(CAFFDRAM_OBJS1) KERNEL-stat_engine.o
	$(CXX) -o$@ $^ $(LDFLAGS)

rankTest: rankTest.o $(CAFFDRAM_OBJS1) KERNEL-stat_engine.o
	$(CXX) -o$@ $^ $(LDFLAGS)

mcMapTest: mcMapTest.o CAFFDRAM-McMap.o CAFFDRAM-Dsettings.o
	$(CXX) -o$@ $^ $(LDFLAGS)



KERNEL-%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o KERNEL-$*.o

CAFFDRAM-%.o: %.cpp
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o CAFFDRAM-$*.o

%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o $*.o


-include $(wildcard dep/*.d)

.PHONY: clean
clean:
	rm -f $(EXECS) *.o DBG* *.gcda *.gcno *.info
	rm -rf dep
