CXX = mpic++
CPPFLAGS += -DKERNEL_UTEST -DNO_MPI -I/usr/include/cppunit -I../..
CPPFLAGS_MESSENGER = -DKERNEL_UTEST -I/usr/include/cppunit -I../..
LDFLAGS += -lcppunit
EXECS = ClockTest ClockTest2 ComponentTest dvfsTest LinkTest LinkOutputTest LinkOutputTest2 ManifoldConnectTest ManifoldScheduleTest ManifoldTest \
        MessengerTest0 MessengerTest_big_data1 tickObjTest 

VPATH = ../..

# If the kernel directory already has an object, say clock.o, then the object is
# not built. This may be wrong. So we use different names for kernel objects. This
# way the objects in the kernel directory are not picked up.
KERNEL_OBJS1 = KERNEL_clock.o KERNEL_component.o KERNEL_manifold.o KERNEL_scheduler.o KERNEL_stat_engine.o KERNEL_syncalg.o KERNEL_lookahead.o
KERNEL_OBJS2 = KERNEL_clock.o KERNEL_component.o KERNEL_link.o KERNEL_manifold.o KERNEL_scheduler.o KERNEL_stat_engine.o KERNEL_syncalg.o KERNEL_lookahead.o
KERNEL_OBJS3 = KERNEL_clock.o KERNEL_link.o KERNEL_manifold.o KERNEL_scheduler.o KERNEL_stat_engine.o KERNEL_syncalg.o KERNEL_lookahead.o


ALL: $(EXECS)

ClockTest: ClockTest.o  $(KERNEL_OBJS1)
	$(CXX) -o$@ $(LDFLAGS) $^

ClockTest2: ClockTest2.o  $(KERNEL_OBJS1)
	$(CXX) -o$@ $(LDFLAGS) $^

ComponentTest: ComponentTest.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $(LDFLAGS) $^

dvfsTest: dvfsTest.o  $(KERNEL_OBJS1)
	$(CXX) -o$@ $(LDFLAGS) $^

LinkTest: LinkTest.o  $(KERNEL_OBJS3)
	$(CXX) -o$@ $(LDFLAGS) $^

LinkOutputTest: LinkOutputTest.o  $(KERNEL_OBJS3)
	$(CXX) -o$@ $(LDFLAGS) $^

LinkOutputTest2: LinkOutputTest2.o  $(KERNEL_OBJS3)
	$(CXX) -o$@ $(LDFLAGS) $^

LinkOutputTest2.o: LinkOutputTest2.cc
	$(CXX) $(CPPFLAGS_MESSENGER) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o $*.o

ManifoldTest: ManifoldTest.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $(LDFLAGS) $^

ManifoldConnectTest: ManifoldConnectTest.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $(LDFLAGS) $^

ManifoldScheduleTest: ManifoldScheduleTest.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $(LDFLAGS) $^

tickObjTest: tickObjTest.o  KERNEL_clock.o KERNEL_stat_engine.o
	$(CXX) -o$@ $(LDFLAGS) $^

MessengerTest0: MessengerTest0.o KERNEL_messenger.o
	$(CXX) -o$@ $(LDFLAGS) $^

MessengerTest0.o: MessengerTest0.cc
	$(CXX) $(CPPFLAGS_MESSENGER) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o $*.o

MessengerTest_big_data1: MessengerTest_big_data1.o  KERNEL_messenger.o
	$(CXX) -o$@ $(LDFLAGS) $^

MessengerTest_big_data1.o: MessengerTest_big_data1.cc
	$(CXX) $(CPPFLAGS_MESSENGER) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o $*.o

MessengerTest_big_data2: MessengerTest_big_data2.o  KERNEL_messenger.o
	$(CXX) -o$@ $(LDFLAGS) $^

KERNEL_messenger.o: messenger.cc
	$(CXX) $(CPPFLAGS_MESSENGER) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o $*.o

KERNEL_%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o KERNEL_$*.o

%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o $*.o

-include $(wildcard dep/*.d)

.PHONY: clean
clean:
	rm -f $(EXECS) *.o DBG*
	rm -rf dep
