configure_file(
  input: 'org.jaos.SlaptService.service.in',
  output: 'org.jaos.SlaptService.service',
  configuration: configuration,
  install_dir: dbus.get_pkgconfig_variable('system_bus_services_dir'),
)

configure_file(
  input: 'SlaptService.conf.in',
  output: 'SlaptService.conf',
  configuration: configuration,
  install_dir: join_paths(dbus.get_pkgconfig_variable('datadir'), 'dbus-1', 'system.d'),
)
