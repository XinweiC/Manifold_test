MAINTAINERCLEANFILES= aclocal.m4 config.log config.status configure depcomp \
		    Makefile.in INSTALL install-sh missing 

SUBDIRS = kernel  uarch  models/cache/mcp-cache  models/cache/simple-cache  models/memory/CaffDRAM  models/network/iris  models/processor/spx  models/cross/mcp_cache-iris
#models/memory/simple-mc  
#models/network/simple-net  

EXTRA_DIST = util/traceGen simulator/smp simulator/smp2 doc/doxygen

dist-hook:
	find $(distdir)/simulator -name '.svn' | xargs rm -rf
	find $(distdir)/doc -name '.svn' | xargs rm -rf
