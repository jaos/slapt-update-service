#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([slapt-update-service], [0.5.0b], [woodwardj@jaos.org])
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
AC_CHECK_LIB(curl,curl_global_init, [LIBS="$LIBS `curl-config --libs`"], [AC_MSG_ERROR(curl libs are missing)])
AC_CHECK_LIB(gpgme,gpgme_new, [
    LIBS="`gpgme-config --libs` $LIBS"
    AC_CHECK_LIB(slapt,slapt_add_pkg_source_gpg_key,
      [AC_DEFINE(SLAPT_HAS_GPGME, 1, [libslapt has builtin gpgme support])],
      [AC_MSG_WARN(libslapt does not have gpgme support)])
], [AC_MSG_WARN(gpg support disabled)])
AC_CHECK_LIB(slapt,slapt_disk_space_check, [], [AC_MSG_ERROR(libslapt 0.10.2b or greater is required)])

PKG_CHECK_MODULES(GLIB, [glib-2.0 gobject-2.0 gthread-2.0])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)
PKG_CHECK_MODULES(DBUS, [dbus-1 dbus-glib-1])
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)
PKG_CHECK_MODULES(GTK, [gtk+-2.0])
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)
PKG_CHECK_MODULES(NOTIFY, [libnotify])
AC_SUBST(NOTIFY_CFLAGS)
AC_SUBST(NOTIFY_LIBS)

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

# options
AC_ARG_WITH(
  [gnomesu], 
  AC_HELP_STRING([--with-gnomesu], [use gnomesu (default is auto)]),
  [ac_cv_prog_GNOMESU=$withval],
  [AC_CHECK_PROG([GNOMESU], [gnomesu], yes, [no])]
)
AC_ARG_WITH(
  [gksu], 
  AC_HELP_STRING([--with-gksu], [use gksu (default is auto)]),
  [ac_cv_prog_GKSU=$withval],
  [AC_CHECK_PROG([GKSU], [gksu], yes, [no])]
)
AC_ARG_WITH(
  [kdesu], 
  AC_HELP_STRING([--with-kdesu], [use kdesu (default is auto)]),
  [ac_cv_prog_KDESU=$withval],
  [AC_CHECK_PROG([KDESU], [kdesu], yes, [no])]
)

if test "$ac_cv_prog_GNOMESU" = "yes"; then
  AC_DEFINE(HAS_GNOMESU, 1, [has gnomesu])
fi
if test "$ac_cv_prog_GKSU" = "yes"; then
  AC_DEFINE(HAS_GKSU, 1, [has gksu])
fi
if test "$ac_cv_prog_KDESU" = "yes"; then
  AC_DEFINE(HAS_KDESU, 1, [has kdesu])
fi
if test "$ac_cv_prog_GNOMESU" = "no" -a "$ac_cv_prog_GKSU" = "no" -a "$ac_cv_prog_KDESU" = "no"; then
  AC_MSG_ERROR([gnomesu, gksu, or kdesu is required.  please specify one with --with-APPNAME=yes.])
fi

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
