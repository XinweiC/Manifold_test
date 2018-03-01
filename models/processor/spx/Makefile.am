CXX = mpic++ -g -Wall -O3

MAINTAINERCLEANFILES=  aclocal.m4 config.log config.status configure COPYING depcomp \
		      INSTALL install-sh Makefile.in missing

spxlib_LIBRARIES = libspx.a
spxlibdir = $(libdir)/manifold

libspx_a_SOURCES = \
	instruction.cc \
	component.cc \
	pipeline.cc \
	pipeline/outorder.cc \
	pipeline/inorder.cc \
	interrupt_handler.cc \
	core.cc 

pkginclude_spxdir = $(includedir)/manifold/spx

pkginclude_spx_HEADERS = \
	instruction.h \
	component.h \
	pipeline.h \
	pipeline/outorder.h \
	pipeline/inorder.h \
	interrupt_handler.h \
	core.h 

libspx_a_CPPFLAGS = -I$(KERNEL_INC) -I$(QSIM_INC) -Ipipeline -lqsim
