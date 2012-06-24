AC_INIT([prevodb], 0.1)

AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([1.11 foreign])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PROG_CC

CFLAGS="$CFLAGS -Wcast-align -Wuninitialized -Wno-strict-aliasing -Wempty-body -Wformat -Wformat-security -Winit-self -Wvla -Wpointer-arith -Wmissing-declarations -Wdeclaration-after-statement"

PKG_CHECK_MODULES(GLIB, [glib-2.0])

AC_CHECK_HEADER([expat.h], [have_expat=yes], [have_expat=no])
AC_CHECK_LIB([expat], [XML_ParserCreate], [], [have_expat=no])

AS_IF([test "x$have_expat" = "xno"], [AC_MSG_ERROR([Expat is required])])

AC_CONFIG_FILES([
        Makefile
        src/Makefile
])

AC_OUTPUT
