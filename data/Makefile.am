
# Dbus service file
servicedir = $(DBUS_SYSTEM_SERVICES_DIR)
service_in_files = org.jaos.SlaptService.service.in
service_DATA = $(service_in_files:.service.in=.service)

# Dbus configuration file
configdir = $(DBUS_SYSCONFIG_DIR)
config_in_files = SlaptService.conf.in
config_DATA = $(config_in_files:.conf.in=.conf)

EXTRA_DIST = $(config_in_files) $(service_in_files)

# Rule to make the service file with sbindir expanded
$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(sbindir)|" $<> $@

# Rule to make the dbus configuration file
$(config_DATA): $(config_in_files) Makefile
	@sed -e "" $<> $@

