/*
 *  GTetrinet
 *  Copyright (C) 1999, 2000, 2001, 2002, 2003  Ka-shu Wong (kswong@zip.com.au)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
# include "../config.h"
#endif

#include <gtk/gtk.h>
#include <gnome.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "sound.h"

extern char **environ;

int soundenable, midienable;

char soundfiles[S_NUM][1024];
char midifile[1024];
char midicmd[1024];

#ifdef HAVE_ESD

#include <esd.h>
#include <libgnome/gnome-sound.h>

static int soundsamples[S_NUM];
static int midipid = 0;

void sound_cache (void)
{
    int i;
    if (!soundenable) return;
    for (i = 0; i < S_NUM; i ++) {
        if (soundsamples[i])
            esd_sample_free (gnome_sound_connection_get (), soundsamples[i]);
        if (soundfiles[i][0])
            soundsamples[i] = gnome_sound_sample_load (soundfiles[i], soundfiles[i]);
        else
            soundsamples[i] = 0;
    }
}

void sound_playsound (int id)
{
    if (!soundenable) return;
    if (soundsamples[id] > 0)
      esd_sample_play (gnome_sound_connection_get (), soundsamples[id]);
}

void sound_playmidi (char *file)
{
#ifdef HAVE_PUTENV
#endif
	
    sound_stopmidi();
    if (!midienable) return;
    if (file[0] == 0) return;
    if ((midipid = fork()) == 0) {
        setsid ();
#ifdef HAVE_SETENV
        setenv ("MIDIFILE", file, TRUE);
#elif HAVE_PUTENV
      {
        char sz[1024];
        g_snprintf(sz, sizeof(sz), "MIDIFILE=%s", file);
        putenv(sz);
      }
#else
#error Need either setenv() or putenv()
#endif
        execl ("/bin/sh", "sh", "-c", midicmd, NULL);
        _exit(0);
    }
}

void sound_stopmidi (void)
{
    if (midipid) {
        kill (-midipid, SIGTERM);
        waitpid (-midipid, NULL, 0);
        midipid = 0;
    }
}

#else

/* stubs */
void sound_cache (void) {}
void sound_playsound (int id) {id = id;}
void sound_playmidi (char *file) {file = file;}
void sound_stopmidi (void) {}

#endif
