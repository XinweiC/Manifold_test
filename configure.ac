AC_PREREQ([2.59])
AC_INIT([manifold], [0.11.1])
AM_INIT_AUTOMAKE([-Wall -Werror])
#LT_PREREQ([2.2])
#LT_INIT

AC_PROG_CXX
AC_PROG_RANLIB


AC_CONFIG_SUBDIRS([kernel
                   uarch
                   models/cache/mcp-cache 
                   models/cache/simple-cache 
                   models/memory/CaffDRAM
                   models/network/iris 
                   models/processor/spx
                   models/cross/mcp_cache-iris
		   ])

#models/memory/simple-mc 
#models/network/simple-net 

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
