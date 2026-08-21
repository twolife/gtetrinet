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
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include "sound.h"

extern char **environ;

int soundenable;

char soundfiles[S_NUM][1024];

static GtkMediaStream *soundsamples[S_NUM] = {NULL};

void sound_cache (void)
{
    int i;

    for (i = 0; i < S_NUM; i ++) {
        g_clear_object (&soundsamples[i]);

        if (soundfiles[i][0] != '\0') {
            soundsamples[i] =
                gtk_media_file_new_for_filename (soundfiles[i]);

            /*
             * Game sounds are short one-shot samples.  Do not loop them and
             * keep them at the normal stream volume.
             */
            gtk_media_stream_set_loop (soundsamples[i], FALSE);
            gtk_media_stream_set_volume (soundsamples[i], 1.0);
        }
    }
}

void sound_playsound (int id)
{
    GtkMediaStream *stream;

    if (!soundenable)
        return;

    if (id < 0 || id >= S_NUM)
        return;

    stream = soundsamples[id];
    if (stream == NULL)
        return;

    /*
     * Rewind before starting so the same effect can be triggered repeatedly.
     * GtkMediaStream uses microseconds; timestamp 0 is the beginning.
     */
    if (gtk_media_stream_is_seekable (stream))
        gtk_media_stream_seek (stream, 0);

    gtk_media_stream_set_playing (stream, TRUE);
}
