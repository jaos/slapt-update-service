bin_PROGRAMS = slapt-update-notifier
sbin_PROGRAMS = slaptService
slaptService_SOURCES = server.c slaptService.c  slaptService.h common.h
slaptService_LDADD = $(GLIB_LIBS) $(DBUS_LIBS)
slaptService_CPPFLAGS = $(GLIB_CFLAGS) $(DBUS_CFLAGS)
slapt_update_notifier_SOURCES = client.c slaptService.c  slaptService.h common.h
slapt_update_notifier_LDADD = $(GLIB_LIBS) $(DBUS_LIBS) $(GTK_LIBS) $(NOTIFY_LIBS)
slapt_update_notifier_CPPFLAGS = $(GLIB_CFLAGS) $(DBUS_CFLAGS) $(GTK_CFLAGS) $(NOTIFY_CFLAGS)

BUILT_SOURCES = server-bindings.h client-bindings.h
# We don't want to install this header
noinst_HEADERS = $(BUILT_SOURCES)

# Correctly clean the generated headers, but keep the xml description
CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = slapt-service.xml

#Rule to generate the binding headers
server-bindings.h:  slapt-service.xml
	dbus-binding-tool --prefix=slapt_service --mode=glib-server $<> $@

client-bindings.h:  slapt-service.xml
	dbus-binding-tool --prefix=slapt_service --mode=glib-client $<> $@

