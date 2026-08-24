SUBDIRS = images

localedir = $(datadir)/locale

gtetrinet_LDADD = $(glib_LIBS) $(gtk_LIBS) $(libsoup_LIBS)

AM_CPPFLAGS = \
	-DGTETRINET_DATA=\"$(pkgdatadir)\"                      \
	-DGTETPIXMAPSDIR=\""$(datadir)/pixmaps/gtetrinet"\"     \
	-DLOCALEDIR=\"$(localedir)\"                            \
	-DPIXMAPSDIR=\""$(datadir)/pixmaps"\"                   \
	-DGSETTINGSSCHEMADIR=\"$(gsettingsschemadir)\"          \
	$(glib_CFLAGS) $(gtk_CFLAGS) $(libsoup_CFLAGS)

bin_PROGRAMS = gtetrinet

gtetrinet_SOURCES = \
	client.c \
	client.h \
	commands.c \
	commands.h \
	config.c \
	dialogs.c \
	dialogs.h \
	fields.c \
	fields.h \
	gtet_config.h \
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
