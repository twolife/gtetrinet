SUBDIRS = images

localedir = $(datadir)/locale

AM_CFLAGS = $(GTETRINET_CFLAGS)

if HAVE_ESOUND
gtetrinet_LDADD = $(GTETRINET_LIBS) $(ESOUND_LIBS)
else
gtetrinet_LDADD = $(GTETRINET_LIBS)
endif

AM_CPPFLAGS = \
	-DGTETRINET_DATA=\"$(pkgdatadir)\"                      \
	-DGTETPIXMAPSDIR=\""$(datadir)/pixmaps/gtetrinet"\"     \
	-DLOCALEDIR=\"$(localedir)\"                            \
	-DPIXMAPSDIR=\""$(datadir)/pixmaps"\"

gamesdir = $(prefix)/games
games_PROGRAMS = gtetrinet

gtetrinet_SOURCES = \
	client.c \
	client.h \
	commands.c \
	commands.h \
	config.c \
	config.h \
	dialogs.c \
	dialogs.h \
	fields.c \
	fields.h \
	gtetrinet.c \
	gtetrinet.h \
	misc.c \
	misc.h \
	partyline.c \
	partyline.h \
	sound.c \
	sound.h \
	tetrinet.c \
	tetrinet.h \
	tetris.c \
	tetris.h \
	winlist.c \
	winlist.h

-include $(top_srcdir)/git.mk
