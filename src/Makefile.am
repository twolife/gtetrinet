SUBDIRS = images

localedir = $(datadir)/locale
INCLUDES = -DLOCALEDIR=\"$(localedir)\" $(GTK_CFLAGS) $(GNOME_INCLUDEDIR)
LDADD = $(GNOME_LIBDIR) $(GNOMEUI_LIBS) $(GTK_LIBS)
AM_CPPFLAGS = -DGTETRINET_DATA=\"$(pkgdatadir)\"

gamesdir = $(prefix)/games
games_PROGRAMS = gtetrinet

gtetrinet_SOURCES = client.c client.h commands.c commands.h config.c config.h dialogs.c dialogs.h fields.c fields.h gtetrinet.c gtetrinet.h keys.c keys.h keytable.h misc.c misc.h partyline.c partyline.h sound.c sound.h tetrinet.c tetrinet.h tetris.c tetris.h winlist.c winlist.h
