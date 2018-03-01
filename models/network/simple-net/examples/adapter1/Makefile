CXX = mpic++ -Wall
CXXFLAGS += -I ../../../../.. -I../../..
LDFLAGS = -L../.. -lsimple-net -L../../../../../kernel -lmanifold

EXECS = adapter1


VPATH = ../..

ALL: $(EXECS)


adapter1: main.o client.o
	$(CXX) -o$@ $^ $(LDFLAGS)

%.o: %.cc
	@[ -d dep ] || mkdir dep
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MF dep/$*.d -c $< -o $*.o

-include $(wildcard dep/*.d)

.PHONY: clean
clean:
	rm -f $(EXECS) *.d *.o DBG*
	rm -rf dep
