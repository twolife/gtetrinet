SUBDIRS= doc po src themes icons

icon_DATA = gtetrinet.png
icondir = $(datadir)/pixmaps

schemadir = $(sysconfdir)/gconf/schemas
schema_in_files = gtetrinet.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

Applicationsdir = $(datadir)/applications
Applications_in_files = gtetrinet.desktop.in
Applications_DATA = $(Applications_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = ABOUT-NLS AUTHORS COPYING ChangeLog INSTALL NEWS README	\
	     TODO config.guess config.sub config.rpath install-sh	\
	     intltool-extract.in intltool-merge.in intltool-update.in \
	     missing mkinstalldirs $(icon_DATA) $(schema_DATA) $(Applications_DATA)

install-data-local:
	-GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA)

changelog:
	cvs2cl -b --utc -S --no-wrap -f ChangeLog.CVS
