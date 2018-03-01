CXX = mpic++ -Wall
CXXFLAGS += -DKERNEL_UTEST -DSTATS -I/usr/include/cppunit -I../..
LDFLAGS += -lcppunit

EXECS = CmbTest1 CmbTest2 CmbTest3  CmbTest4  LBTSTest1 LBTSTest2 LBTSTest3   ManifoldConnectTest  ManifoldConnectTest2  MessagingClockTest MessagingClockTest_ser MessagingClockTest_ser_pointer MessagingHalfClockTest MessagingHalfClockTest_ser MessagingHalfClockTest_ser_pointer MessagingHalfTest MessagingHalfTest_ser MessagingHalfTest_ser_pointer MessagingTest MessagingTest_ser MessagingTest_ser_pointer MessagingTest_ser_pointer2 MessagingTimeTest MessagingTimeTest_ser MessagingTimeTest_ser_pointer  QtmTest4  SchedulerTestTerminate  SchedulerTestTerminate2


VPATH = ../..

# If the kernel directory already has an object, say clock.o, then the object is
# not built. This may be wrong. So we use different names for kernel objects. This
# way the objects in the kernel directory are not picked up.
KERNEL_OBJS1 = KERNEL_clock.o KERNEL_component.o KERNEL_manifold.o KERNEL_messenger.o KERNEL_quantum_scheduler.o KERNEL_scheduler.o  KERNEL_stat_engine.o KERNEL_syncalg.o KERNEL_lookahead.o
KERNEL_OBJS2 = $(KERNEL_OBJS1) KERNEL_link.o


ALL: $(EXECS)



CmbTest1: CmbTest1.o  $(KERNEL_OBJS1)
	$(CXX) -o$@ $^ $(LDFLAGS)

CmbTest2: CmbTest2.o  $(KERNEL_OBJS1)
	$(CXX) -o$@ $^ $(LDFLAGS)

CmbTest3: CmbTest3.o  $(KERNEL_OBJS1)
	$(CXX) -o$@ $^ $(LDFLAGS)

CmbTest4: CmbTest4.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

LBTSTest1: LBTSTest1.o  $(KERNEL_OBJS1)
	$(CXX) -o$@ $^ $(LDFLAGS)

LBTSTest2: LBTSTest2.o  $(KERNEL_OBJS1)
	$(CXX) -o$@ $^ $(LDFLAGS)

LBTSTest3: LBTSTest3.o  $(KERNEL_OBJS1)
	$(CXX) -o$@ $^ $(LDFLAGS)

ManifoldConnectTest: ManifoldConnectTest.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

ManifoldConnectTest2: ManifoldConnectTest2.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingClockTest: MessagingClockTest.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingClockTest_ser: MessagingClockTest_ser.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingClockTest_ser_pointer: MessagingClockTest_ser_pointer.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingHalfClockTest: MessagingHalfClockTest.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingHalfClockTest_ser: MessagingHalfClockTest_ser.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingHalfClockTest_ser_pointer: MessagingHalfClockTest_ser_pointer.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingHalfTest: MessagingHalfTest.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingHalfTest_ser: MessagingHalfTest_ser.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingHalfTest_ser_pointer: MessagingHalfTest_ser_pointer.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingTest: MessagingTest.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingTest_ser: MessagingTest_ser.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingTest_ser_pointer: MessagingTest_ser_pointer.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingTest_ser_pointer2: MessagingTest_ser_pointer2.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingTimeTest: MessagingTimeTest.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingTimeTest_ser: MessagingTimeTest_ser.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

MessagingTimeTest_ser_pointer: MessagingTimeTest_ser_pointer.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

QtmTest4: QtmTest4.o  $(KERNEL_OBJS2)
	$(CXX) -o$@ $^ $(LDFLAGS)

SchedulerTestTerminate: SchedulerTestTerminate.o  $(KERNEL_OBJS1)
	$(CXX) -o$@ $^ $(LDFLAGS)

SchedulerTestTerminate2: SchedulerTestTerminate2.o  $(KERNEL_OBJS1)
	$(CXX) -o$@ $^ $(LDFLAGS)


force: ;

KERNEL_%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o KERNEL_$*.o

%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o $*.o

-include $(wildcard dep/*.d)

.PHONY: clean
clean:
	rm -f $(EXECS) *.d *.o DBG*
	rm -rf dep
