CXX = mpic++
#For distribution, the following 2 lines should be uncommented.
#CPPFLAGS += -Wall -g -I/usr/local/include/manifold
#LDFLAGS = -L/usr/local/lib/manifold/iris -liris -lmanifold

#For distribution, the following 3 lines should be commented.
MODELS_DIR = ../../../../models
CPPFLAGS += -Wall -g -DSTATS -I ../../../..  -I$(MODELS_DIR)/network
LDFLAGS = -L$(MODELS_DIR)/network/iris -liris  -L../../../../kernel -lmanifold


EXECS = RingExample1


ALL: $(EXECS)


RingExample1: RingExample1.o
	$(CXX) $^ -o$@  $(LDFLAGS)


%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o $*.o

-include $(wildcard dep/*.d)

.PHONY: clean
clean:
	rm -f $(EXECS) *.d *.o
	rm -rf dep
