CXX = mpic++ #-DDBG_MESI_CLIENT  -DDBG_MESI_MANAGER -DDBG_MCP_CACHE_L1_CACHE -DDBG_MCP_CACHE_L2_CACHE
#CPPFLAGS += -g -fprofile-arcs -ftest-coverage -DMCP_CACHE_UTEST -DKERNEL_UTEST -DNO_MPI -I/usr/include/cppunit -I../.. -I../../../../..
CPPFLAGS += -g -DMCP_CACHE_UTEST -DKERNEL_UTEST -DNO_MPI -I/usr/include/cppunit -I../.. -I../../coherence -I../../../../..
LDFLAGS += -lcppunit #-lgcov
EXECS = coh_mem_reqTest  hash_entryTest  hash_setTest  hash_tableTest  sharersTest \
    MESI_L1_cacheFlowControlTest  MESI_L1_cacheTest  MESI_L2_cacheFlowControlTest  MESI_L2_cacheTest  MESI_LLP_cacheFlowControlTest  MESI_LLP_cacheTest  MESI_LLS_cacheFlowControlTest  MESI_LLS_cacheTest  MESI_clientTest  MESI_managerTest

VPATH = ../..  ../../coherence  ../../../../../kernel


MCPCACHE_OBJS = MCPCACHE-cache_req.o  MCPCACHE-hash_table.o  MCPCACHE-L1_cache.o  MCPCACHE-L2_cache.o  MCPCACHE-MESI_L1_cache.o  MCPCACHE-MESI_L2_cache.o  MCPCACHE-LLP_cache.o  MCPCACHE-MESI_LLP_cache.o  MCPCACHE-LLS_cache.o  MCPCACHE-MESI_LLS_cache.o  MCPCACHE-mux_demux.o
MESI_OBJS = MCPCACHE-ClientInterface.o  MCPCACHE-ManagerInterface.o  MCPCACHE-MESI_client.o  MCPCACHE-MESI_manager.o  MCPCACHE-sharers.o
KERNEL_OBJS = KERNEL-component.o KERNEL-manifold.o KERNEL-clock.o KERNEL-scheduler.o KERNEL-stat_engine.o KERNEL-syncalg.o KERNEL-lookahead.o

ALL: $(EXECS)

coh_mem_reqTest: coh_mem_reqTest.o  MCPCACHE-coh_mem_req.o
	$(CXX) -o$@ $^ $(LDFLAGS)

hash_entryTest: hash_entryTest.o  MCPCACHE-hash_table.o
	$(CXX) -o$@ $^ $(LDFLAGS)

hash_setTest: hash_setTest.o  MCPCACHE-hash_table.o
	$(CXX) -o$@ $^ $(LDFLAGS)

hash_tableTest: hash_tableTest.o  MCPCACHE-hash_table.o
	$(CXX) -o$@ $^ $(LDFLAGS)

MESI_L1_cacheTest: MESI_L1_cacheTest.o  $(MCPCACHE_OBJS) $(MESI_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

MESI_L1_cacheFlowControlTest: MESI_L1_cacheFlowControlTest.o  $(MCPCACHE_OBJS) $(MESI_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

MESI_L2_cacheTest: MESI_L2_cacheTest.o  $(MCPCACHE_OBJS) $(MESI_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

MESI_L2_cacheFlowControlTest: MESI_L2_cacheFlowControlTest.o  $(MCPCACHE_OBJS) $(MESI_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

MESI_LLP_cacheTest: MESI_LLP_cacheTest.o  $(MCPCACHE_OBJS) $(MESI_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

MESI_LLP_cacheFlowControlTest: MESI_LLP_cacheFlowControlTest.o  $(MCPCACHE_OBJS) $(MESI_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

MESI_LLS_cacheTest: MESI_LLS_cacheTest.o  $(MCPCACHE_OBJS) $(MESI_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

MESI_LLS_cacheFlowControlTest: MESI_LLS_cacheFlowControlTest.o  $(MCPCACHE_OBJS) $(MESI_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

MESI_clientTest: MESI_clientTest.o  $(MESI_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

MESI_managerTest: MESI_managerTest.o  $(MESI_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

sharersTest: sharersTest.o  MCPCACHE-sharers.o
	$(CXX) -o$@ $^ $(LDFLAGS)



MCPCACHE-%.o: %.cpp
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o MCPCACHE-$*.o

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
