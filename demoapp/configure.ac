AC_INIT([demoapp],[4.9.3],[])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_PREFIX_DEFAULT(["/opt/netgen"])


AC_ARG_WITH([netgen], 
        [  --with-netgen=dir       use Netgen installed in directory dir],
        [AC_SUBST([NETGEN_INCLUDES], ["-I$withval"])]
        )

AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

AC_CONFIG_HEADERS(config.h) 
AC_CONFIG_FILES(Makefile)

AC_OUTPUT
