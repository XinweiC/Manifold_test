CXX = mpic++ -Wall

MAINTAINERCLEANFILES=  aclocal.m4 config.log config.status configure COPYING depcomp \
		      INSTALL install-sh Makefile.in missing

simple_mclib_LIBRARIES = libsimple-mc.a
simple_mclibdir = $(libdir)/manifold
libsimple_mc_a_SOURCES = \
	simple_mc.cc \
	simple_mc.h

pkginclude_simple_mcdir = $(includedir)/manifold/simple-mc

pkginclude_simple_mc_HEADERS = \
	simple_mc.h

# simple-mc use MemoryControllerMap.h from simple-cache. If this dependency changes,
# the following line should also change.
libsimple_mc_a_CPPFLAGS = -I$(KERNEL_INC) -I$(SIMPLECACHE_INC)
