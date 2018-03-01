#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.59])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
AM_INIT_AUTOMAKE([manifold_simple_mc], [1.0])
AC_CONFIG_SRCDIR([simple_mc.h])
#AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB

# Checks for libraries.
#AC_CHECK_LIB([qsim], [main])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_UINT64_T

# Checks for library functions.

# configure variables: KERINC
# This can be used to let user tell us where to find kernel headers.
# For example: configure  KERINC="foo/include/kernel"
AC_ARG_VAR([KERINC], [The path where kernel headers can be found.])

# if KERINC not specified, first look in super-project, and if not found,
# then set to installation directory.
if test -z "$KERINC" ; then
    if test -d "$srcdir/../../../kernel"; then
	KERINC='$(top_srcdir)/../../..'
	AC_MSG_NOTICE([
    -----------------------------------------
    Use kernel header files in super-project.
    -----------------------------------------
	])
    else
	#check installation dir
	# unfortunately I can't use if test -f "$includedir/manifold/kernel/manifold.h"
	# because $includedir will only expand to ${prefix}/include

	# simply set the value. I can't check if it's really there.
	KERINC='$(includedir)/manifold'
	AC_MSG_NOTICE([
    --------------------------------------------------
    Use kernel header files in installation direcotry.
    --------------------------------------------------
	])
    fi
else # KERINC is specified
    AC_MSG_NOTICE([
    -----------------------------------------
    Use kernel header files in $KERINC
    -----------------------------------------
    ])
fi

AC_SUBST([KERNEL_INC], [$KERINC])



# simple-mc use MemoryControllerMap.h from simple-cache. If this dependency changes,
# the following should also change.

    if test -d "$srcdir/../../../models/cache/simple-cache"; then
	SIMPCACHEINC='$(top_srcdir)/../../../models/cache'
	AC_MSG_NOTICE([
    -----------------------------------------
    Use simple-cache header files in super-project.
    -----------------------------------------
	])
    else
	# simply set the value. I can't check if it's really there.
	SIMPCACHEINC='$(includedir)/simple-cache'
	AC_MSG_NOTICE([
    --------------------------------------------------
    Use simple-cache header files in installation direcotry.
    --------------------------------------------------
	])
    fi


AC_SUBST([SIMPLECACHE_INC], [$SIMPCACHEINC])


AC_CONFIG_FILES([Makefile])
AC_OUTPUT
