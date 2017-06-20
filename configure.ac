#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(busactd, 4.2, [walyong.cho@samsung.com])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_PREFIX_DEFAULT([/usr])
AM_INIT_AUTOMAKE([foreign subdir-objects])

LT_PREREQ(2.2)
LT_INIT([disable-static])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([limits.h stdlib.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([strcspn strdup strerror strndup])

AC_PATH_PROG([M4], [m4])
M4_DEFINES=

# ------------------------------------------------------------------------------
our_cflags=" \
        -g -O2 \
        -Werror \
        -fpie"

our_ldflags=" \
        -Wl,--as-needed \
        -Wl,--no-undefined \
        -Wl,--gc-sections \
        -Wl,-z,relro \
        -Wl,-z,now \
        -pie"

AC_SUBST([OUR_CFLAGS], "$our_cflags")
AC_SUBST([OUR_LDFLAGS], "$our_ldflags")

# ------------------------------------------------------------------------------
AC_SUBST(M4_DEFINES)

# ------------------------------------------------------------------------------
PKG_CHECK_MODULES(GLIB, [glib-2.0])
PKG_CHECK_MODULES(GIO, [gio-2.0])
PKG_CHECK_MODULES(LIBSYSTEMD, [libsystemd])
PKG_CHECK_MODULES(LIBSYSTEM, [libsystem])

# ------------------------------------------------------------------------------
AC_CONFIG_FILES([Makefile])

AC_OUTPUT
AC_MSG_RESULT([
        $PACKAGE_NAME $VERSION

        OUR CFLAGS:              ${OUR_CFLAGS} ${CFLAGS}
])
