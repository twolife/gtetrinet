SUBDIRS = images

localedir = $(datadir)/locale

INCLUDES = -DLOCALEDIR=\"$(localedir)\" -DPIXMAPSDIR=\""$(datadir)/pixmaps"\" -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGNOME_DISABLE_DEPRECATED $(GTET_CFLAGS)
LDADD = $(GTET_LIBS)
AM_CPPFLAGS = -DGTETRINET_DATA=\"$(pkgdatadir)\"

gamesdir = $(prefix)/games
games_PROGRAMS = gtetrinet

gtetrinet_SOURCES = client.c client.h commands.c commands.h config.c config.h dialogs.c dialogs.h fields.c fields.h gtetrinet.c gtetrinet.h misc.c misc.h partyline.c partyline.h sound.c sound.h tetrinet.c tetrinet.h tetris.c tetris.h winlist.c winlist.h
