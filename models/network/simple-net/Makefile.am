CXX = mpic++

MAINTAINERCLEANFILES=  aclocal.m4 config.log config.status configure COPYING depcomp \
		      INSTALL install-sh Makefile.in missing

simplenetlib_LIBRARIES = libsimple-net.a
simplenetlibdir = $(libdir)/manifold
libsimple_net_a_SOURCES = \
        interfaceCreator.h \
	mapping.cc \
	mapping.h \
	network.cc \
	network.h \
	networkInterface.h \
	networkPkt.h \
	router.cc \
	router.h

pkginclude_simple_netdir = $(includedir)/manifold/simple-net

pkginclude_simple_net_HEADERS = \
        interfaceCreator.h \
	mapping.h \
	network.h \
	networkInterface.h \
	networkPkt.h

libsimple_net_a_CPPFLAGS = -I$(KERNEL_INC)
