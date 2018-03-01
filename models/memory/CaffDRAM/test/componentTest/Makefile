CXX = mpic++  #-fprofile-arcs -ftest-coverage
CPPFLAGS += -g -Wall -DCAFFDRAM_TEST -DKERNEL_UTEST -DNO_MPI -I/usr/include/cppunit -I../.. -I../../../../../ -I../../../../cache
LDFLAGS += -lcppunit

EXECS = bankTest controllerTest 

VPATH = ../.. ../../../../../kernel

# If the kernel directory already has an object, say clock.o, then the object is
# not built. This may be wrong. So we use different names for kernel objects. This
# way the objects in the kernel directory are not picked up.
KERNEL_OBJS = KERNEL-clock.o KERNEL-component.o KERNEL-manifold.o KERNEL-scheduler.o KERNEL-messenger.o KERNEL-stat_engine.o KERNEL-syncalg.o KERNEL-lookahead.o

# Do the same for caffdram
CAFFDRAM_OBJS1 = CAFFDRAM-Bank.o  CAFFDRAM-Dreq.o CAFFDRAM-Dsettings.o
CAFFDRAM_OBJS2 = $(CAFFDRAM_OBJS1) CAFFDRAM-Channel.o  CAFFDRAM-Rank.o
CAFFDRAM_OBJS =  $(CAFFDRAM_OBJS2) CAFFDRAM-Controller.o


ALL: $(EXECS)

bankTest: bankTest.o $(CAFFDRAM_OBJS1) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

controllerTest: controllerTest.o $(CAFFDRAM_OBJS) $(KERNEL_OBJS) 
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
