SUBDIRS= doc po src themes

icon_DATA = gtetrinet.png
icondir = $(datadir)/pixmaps

util_DATA = gtetrinet.desktop
utildir = $(datadir)/gnome/apps/Games

EXTRA_DIST = ABOUT-NLS AUTHORS COPYING ChangeLog INSTALL NEWS README	\
	     TODO config.guess config.sub config.rpath install-sh	\
	     missing mkinstalldirs $(icon_DATA) $(util_DATA)

changelog:
	cvs2cl -b --utc -S --no-wrap -f ChangeLog.CVS
