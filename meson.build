project('slapt-update-service', 'c',
  version: '0.5.3',
  license: 'GPLv2',
  meson_version: '>= 0.53',
  default_options: [
    'buildtype=debugoptimized',
    'sysconfdir=/etc',
    'b_lto=true',
  ],
)
package = meson.project_name()
version = meson.project_version()
i18n = import('i18n')

# paths
prefix = get_option('prefix')
localedir = join_paths(prefix, get_option('localedir'))
datadir = join_paths(prefix, get_option('datadir'))
docdatadir = join_paths(datadir, 'doc', package)
pixmapsdir = join_paths(datadir, 'pixmaps')
sbindir = join_paths(prefix, get_option('sbindir'))

configuration = configuration_data()
configuration.set_quoted('PACKAGE', package)
configuration.set_quoted('GETTEXT_PACKAGE', package)
configuration.set_quoted('VERSION', version)
configuration.set_quoted('PIXMAPS_DIR', pixmapsdir)
configuration.set('servicedir', sbindir)

# compiler based checks
libcurl = dependency('libcurl')
zlib = dependency('zlib')
openssl = dependency('openssl')
libgpgme = dependency('gpgme', required: false)
cc = meson.get_compiler('c')
libm = cc.find_library('m')
# libslapt = dependency('libslapt', version: '>=0.11.1') # use this when we have a few pkg-config enabled releases out
libslapt = cc.find_library('slapt', has_headers: ['slapt.h'])
if libslapt.found()
  # if libgpgme.found() and cc.has_function('slapt_add_pkg_source_gpg_key', dependencies: [libslapt])`
  if libgpgme.found() and cc.has_function('slapt_add_pkg_source_gpg_key', dependencies: [libcurl, zlib, openssl, libgpgme, libslapt])
    configuration.set('SLAPT_HAS_GPGME', 1)
  endif
endif
glib = dependency('glib-2.0')
gobject = dependency('gobject-2.0')
gthread = dependency('gthread-2.0')
gio = dependency('gio-2.0')
gio_unix = dependency('gio-unix-2.0')
gtk3 = dependency('gtk+-3.0', version: '>= 3.4.0')
libnotify = dependency('libnotify', required: false)
if libnotify.found()
  configuration.set('USE_LIBNOTIFY', 1)
endif
pkexec = find_program('pkexec')
gslapt = find_program('gslapt-polkit')
configuration.set_quoted('GSLAPT_POLKIT_PATH', gslapt.path())
dbus = dependency('dbus-1')

service_deps = [libcurl, zlib, openssl, libm, libgpgme, libslapt, glib, gobject, gthread, gio, gio_unix]
client_deps = service_deps + [gtk3, libnotify]

subdir('po')
subdir('src')
subdir('pixmaps')
subdir('data')

install_data(['AUTHORS', 'ChangeLog', 'COPYING', 'INSTALL', 'NEWS', 'README'], install_dir: docdatadir)
install_data('slapt-update-service.desktop', install_dir: join_paths(get_option('sysconfdir'), 'xdg', 'autostart'))
