service_sources = [
  'server.c',
  'slaptService.c',
  'slaptService.h',
  'common.h',
]

client_sources = [
  'client.c',
  'slaptService.c',
  'slaptService.h',
  'common.h',
]



configure_file(output: 'config.h', configuration: configuration)
executable('slaptService', service_sources, dependencies : service_deps, install: true, install_dir: get_option('sbindir'))
executable('slapt-update-notifier', client_sources, dependencies : client_deps, install: true, install_dir: get_option('bindir'), link_args: ['-Wl,--export-dynamic'])
