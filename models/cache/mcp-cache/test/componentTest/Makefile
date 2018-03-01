CXX = mpic++ #-DDBG_MESI_CLIENT -DDBG_MESI_MANAGER -DDBG_MCP_CACHE_L1_CACHE -DDBG_MCP_CACHE_L2_CACHE  -DDBG_MCP_CACHE_LLP_CACHE  -DDBG_MCP_CACHE_LLS_CACHE
CPPFLAGS += -g -DMCP_CACHE_UTEST -DKERNEL_UTEST -DNO_MPI -I/usr/include/cppunit -I../.. -I../../../../..
LDFLAGS += -lcppunit
EXECS = MESI_L1_L2_cacheTest  MESI_LLP_LLS_cacheTest 

VPATH = ../..  ../../coherence  ../../../../../kernel


MCP_CACHE_OBJS = MCPCACHE-cache_req.o  MCPCACHE-coh_mem_req.o  MCPCACHE-hash_table.o  MCPCACHE-L1_cache.o  MCPCACHE-L2_cache.o MCPCACHE-MESI_L1_cache.o  MCPCACHE-MESI_L2_cache.o  MCPCACHE-LLP_cache.o  MCPCACHE-LLS_cache.o  MCPCACHE-MESI_LLP_cache.o  MCPCACHE-MESI_LLS_cache.o  MCPCACHE-mux_demux.o  MCPCACHE-lp_lls_unit.o
MESI_OBJS = MCPCACHE-ClientInterface.o  MCPCACHE-ManagerInterface.o  MCPCACHE-MESI_client.o  MCPCACHE-MESI_manager.o  MCPCACHE-sharers.o
KERNEL_OBJS = KERNEL-component.o KERNEL-manifold.o KERNEL-clock.o KERNEL-scheduler.o KERNEL-stat_engine.o

ALL: $(EXECS)

MESI_L1_L2_cacheTest: MESI_L1_L2_cacheTest.o  $(MCP_CACHE_OBJS) $(MESI_OBJS) $(KERNEL_OBJS)
	$(CXX) -o$@ $^ $(LDFLAGS)

MESI_LLP_LLS_cacheTest: MESI_LLP_LLS_cacheTest.o  $(MCP_CACHE_OBJS) $(MESI_OBJS) $(KERNEL_OBJS)
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
