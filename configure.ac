#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([slapt-update-service], [0.5.3], [woodwardj@jaos.org])
AM_INIT_AUTOMAKE([-Wall foreign])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_HEADERS([config.h])
IT_PROG_INTLTOOL([0.35.0])

AC_CANONICAL_HOST

case "$host_cpu" in
i[[3456]]86) host_cpu=i386 ;CFLAGS="$CFLAGS -fpic";;
x86_64) host_cpu=x86_64 ;CFLAGS="$CFLAGS -fPIC";;
esac

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_HEADER(slapt.h, [], [AC_MSG_ERROR(libslapt headers are missing)])
AC_CHECK_LIB(z,gzopen, [LIBS="$LIBS -lz"], [AC_MSG_ERROR(libz is missing)])
AC_CHECK_LIB(m,round, [LIBS="$LIBS -lm"], [AC_MSG_ERROR(libm is missing)])
AC_CHECK_LIB(curl,curl_global_init, [LIBS="$LIBS `curl-config --libs`"], [AC_MSG_ERROR(curl libs are missing)])
AC_CHECK_LIB(gpgme,gpgme_new, [
    LIBS="`gpgme-config --libs` $LIBS"
    AC_CHECK_LIB(slapt,slapt_add_pkg_source_gpg_key,
      [AC_DEFINE(SLAPT_HAS_GPGME, 1, [libslapt has builtin gpgme support])],
      [AC_MSG_WARN(libslapt does not have gpgme support)])
], [AC_MSG_WARN(gpg support disabled)])
AC_CHECK_LIB(slapt,slapt_vector_t_init, [], [AC_MSG_ERROR(libslapt 0.11.1 or greater is required)])

PKG_CHECK_MODULES(GLIB, [glib-2.0 gobject-2.0 gthread-2.0 gio-2.0 gio-unix-2.0])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(GTK, [gtk+-3.0 >= 3.4.0])
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

AC_ARG_WITH(
  [notification],
  AC_HELP_STRING([--with-notification], [use notification (default is no)]),
)
AS_IF([test "x$with_notification" = "xyes"], [
  PKG_CHECK_MODULES(NOTIFY, [libnotify])
  AC_DEFINE(USE_LIBNOTIFY, 1, [has notification])
  AC_SUBST(NOTIFY_CFLAGS)
  AC_SUBST(NOTIFY_LIBS)
])

# too many forgotten overrides... just make it the default
if test "$sysconfdir" = '${prefix}/etc'
then
    sysconfdir=/etc
fi

AS_AC_EXPAND(DATADIR, $datadir)
DBUS_SYSTEM_SERVICES_DIR="$DATADIR/dbus-1/system-services"
AC_SUBST(DBUS_SYSTEM_SERVICES_DIR)
AC_DEFINE_UNQUOTED(DBUS_SYSTEM_SERVICES_DIR, "$DBUS_SYSTEM_SERVICES_DIR", [Where system services dir for DBUS is])

AS_AC_EXPAND(SYSCONFDIR, $sysconfdir)
DBUS_SYSCONFIG_DIR="$SYSCONFDIR/dbus-1/system.d"
AC_SUBST(DBUS_SYSCONFIG_DIR)
AC_DEFINE_UNQUOTED(DBUS_SYSCONFIG_DIR, "$DBUS_SYSCONFIG_DIR", [Where service configuration files for DBUS are installed])

PIXMAPS_DIR="$prefix/share/pixmaps"
AC_SUBST(PIXMAPS_DIR)
AC_DEFINE_UNQUOTED(PIXMAPS_DIR, "$PIXMAPS_DIR", [Where pixmap image files are installed])

AC_PATH_PROG([GSLAPT_POLKIT_PATH], [gslapt-polkit])
AC_SUBST(GSLAPT_POLKIT_PATH)
AC_DEFINE_UNQUOTED(GSLAPT_POLKIT_PATH, "$GSLAPT_POLKIT_PATH", [gslapt-polkit helper path])

# translations
GETTEXT_PACKAGE=slapt-update-notifier
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])
AM_GLIB_GNU_GETTEXT

AC_CONFIG_FILES([
  Makefile
  data/Makefile
  src/Makefile
  po/Makefile.in
])
AC_OUTPUT
