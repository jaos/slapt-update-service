SUBDIRS = po data src

pixmapdir = $(PIXMAPS_DIR)
pixmap_DATA = pixmaps/slapt-update-notifier-update.png pixmaps/slapt-update-notifier-idle.png

SLACK_FILES = doinst.sh slack-desc slack-required
EXTRA_DIST = $(pixmap_DATA) $(SLACK_FILES)

PKGDIR=`pwd`/pkg
pkg: all
	$(MAKE) install DESTDIR=$(PKGDIR)
	$(mkinstalldirs) $(PKGDIR)/install
	install doinst.sh $(PKGDIR)/install/
	install slack-desc $(PKGDIR)/install/
	install slack-required $(PKGDIR)/install/
	-chown -R $$(stat --format "%u:%g" $(bindir)) $(PKGDIR)/$(bindir)
	-chown -R $$(stat --format "%u:%g" $(sbindir)) $(PKGDIR)/$(sbindir)
	-find $(PKGDIR)/$(bindir) $(PKGDIR)/$(sbindir)/ -type f -exec strip --strip-unneeded {} \;
	( cd $(PKGDIR) && /sbin/makepkg -l y -c n ../@PACKAGE@-@PACKAGE_VERSION@-@host_cpu@-1.tgz )

