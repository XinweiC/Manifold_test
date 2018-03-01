CC = g++

MAKE = make
AR = ar qcv
RANLIB = ranlib
RM = rm -rf
OEXT = o
LEXT = a
EEXT =
CFLAGS = -g -O3

SRCS = \
intsim.cc \
chip.cc \
parameters.cc

HDRS = \
intsim.h \
chip.h \
parameters.h

OBJS = \
intsim.$(OEXT) \
chip.$(OEXT) \
parameters.$(OEXT)

default: lib intsim

lib: $(OBJS)
	ar qcv libintsim.a $(OBJS)
	ranlib libintsim.a

intsim$(EEXT): intsim_alpha.$(OEXT) $(OBJS)
	$(CC) -o intsim$(EEXT) intsim_alpha.$(OEXT) $(OBJS)

.cc.$(OEXT):
	$(CC) $(CFLAGS) -o $*.$(OEXT) -c $*.cc

clean:
	$(RM) libintsim.a intsim output.txt *.o *~

