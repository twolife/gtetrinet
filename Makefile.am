SUBDIRS= doc po src themes

icon_DATA = gtetrinet.png
icondir = $(datadir)/pixmaps

schemadir = $(sysconfdir)/gconf/schemas
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
schema_in_files = gtetrinet.schemas.in
@INTLTOOL_SCHEMAS_RULE@

utildir = $(datadir)/gnome/apps/Games
util_in_files = gtetrinet.desktop.in
util_DATA = $(util_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = ABOUT-NLS AUTHORS COPYING ChangeLog INSTALL NEWS README	\
	     TODO config.guess config.sub config.rpath install-sh	\
	     intltool-extract.in intltool-merge.in intltool-update.in \
	     missing mkinstalldirs $(icon_DATA) $(schema_DATA) $(util_DATA)

install-data-local:
	-GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)

changelog:
	cvs2cl -b --utc -S --no-wrap -f ChangeLog.CVS
