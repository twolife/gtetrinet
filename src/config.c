/*
 *  GTetrinet
 *  Copyright (C) 1999, 2000  Ka-shu Wong (kswong@zip.com.au)
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
#include "gtetrinet.h"
#include "client.h"
#include "tetrinet.h"
#include "sound.h"
#include "misc.h"

char blocksfile[1024];
int bsize;

char currenttheme[1024];

static char *soundkeys[S_NUM] = {
    "Sounds/Drop",
    "Sounds/Solidify",
    "Sounds/LineClear",
    "Sounds/Tetris",
    "Sounds/Rotate",
    "Sounds/SpecialLine",
    "Sounds/YouWin",
    "Sounds/YouLose",
    "Sounds/Message",
    "Sounds/GameStart",
};

gint defaultkeys[K_NUM] = {
    GDK_Right,
    GDK_Left,
    GDK_Up,
    GDK_Control_R,
    GDK_Down,
    GDK_space,
    GDK_d,
    GDK_t
};

gint keys[K_NUM];

/* themedir is assumed to have a trailing slash */
void config_loadtheme (char *themedir)
{
    char buf[1024], *p;
    int i;

    GTET_O_STRCPY(buf, "=");
    GTET_O_STRCAT(buf, themedir);
    GTET_O_STRCAT(buf, "theme.cfg=/");

    gnome_config_push_prefix (buf);

    p = gnome_config_get_string ("Theme/Name");
    if (p == 0) {
        GtkWidget *mb;
        mb = gnome_message_box_new (_("Warning: theme does not have a name"),
                                    GNOME_MESSAGE_BOX_WARNING, GNOME_STOCK_BUTTON_OK, NULL);
        gnome_dialog_run (GNOME_DIALOG(mb));
    }
    else g_free (p);

    p = gnome_config_get_string ("Graphics/Blocks=blocks.png");
    GTET_O_STRCPY(blocksfile, themedir);
    GTET_O_STRCAT(blocksfile, p);
    g_free (p);
    bsize = gnome_config_get_int ("Graphics/BlockSize=16");

    p = gnome_config_get_string ("Sounds/MidiFile");
    if (p) {
        GTET_O_STRCPY(midifile, themedir);
        GTET_O_STRCAT(midifile, p);
        g_free (p);
    }
    else
        midifile[0] = 0;

    for (i = 0; i < S_NUM; i ++) {
        p = gnome_config_get_string (soundkeys[i]);
        if (p) {
            GTET_O_STRCPY(soundfiles[i], themedir);
            GTET_O_STRCAT(soundfiles[i], p);
            g_free (p);
        }
        else
            soundfiles[i][0] = 0;
    }

    sound_cache ();

    gnome_config_pop_prefix ();
}

/* Arrggh... all these params are sizeof() == 1024 ... this needs a real fix */
int config_getthemeinfo (char *themedir, char *name, char *author, char *desc)
{
    char buf[1024], *p;

    GTET_O_STRCPY (buf, "=");
    GTET_O_STRCAT (buf, themedir);
    GTET_O_STRCAT (buf, "theme.cfg=/");

    gnome_config_push_prefix (buf);

    p = gnome_config_get_string ("Theme/Name");
    if (p == 0) {
        gnome_config_pop_prefix ();
        return -1;
    }
    else {
        if (name) GTET_STRCPY(name, p, 1024);
        g_free (p);
    }
    if (author) {
        p = gnome_config_get_string ("Theme/Author=");
        GTET_STRCPY(author, p, 1024);
        g_free (p);
    }
    if (desc) {
        p = gnome_config_get_string ("Theme/Description=");
        GTET_STRCPY(desc, p, 1024);
        g_free (p);
    }

    gnome_config_pop_prefix ();

    return 0;
}

void config_loadconfig (void)
{
    char *p;
    int k, l;

    gnome_config_push_prefix ("/"APPID"/");

    p = gnome_config_get_string ("Themes/ThemeDir="DEFAULTTHEME);
    GTET_O_STRCPY(currenttheme, p);
    g_free (p);
    /* add trailing slash if none exists */
    l = strlen(currenttheme);
    if (currenttheme[l-1] != '/') {
      GTET_O_STRCAT(currenttheme, "/");
    }

    p = gnome_config_get_string ("Sound/MidiPlayer="DEFAULTMIDICMD);
    GTET_O_STRCPY(midicmd, p);
    g_free (p);

    soundenable = gnome_config_get_int ("Sound/EnableSound=1");
    midienable = gnome_config_get_int ("Sound/EnableMidi=1");

    p = gnome_config_get_string ("Player/Nickname");
    if (p) {
        GTET_O_STRCPY(nick, p);
        g_free(p);
    }

    p = gnome_config_get_string ("Player/Server");
    if (p) {
        GTET_O_STRCPY(server, p);
        g_free(p);
    }

    p = gnome_config_get_string ("Player/Team");
    if (p) {
        GTET_O_STRCPY(team, p);
        g_free(p);
    }

    k = gnome_config_get_int ("Keys/Right");
    keys[K_RIGHT] = k ? k : defaultkeys[K_RIGHT];
    k = gnome_config_get_int ("Keys/Left");
    keys[K_LEFT] = k ? k : defaultkeys[K_LEFT];
    k = gnome_config_get_int ("Keys/RotateRight");
    keys[K_ROTRIGHT] = k ? k : defaultkeys[K_ROTRIGHT];
    k = gnome_config_get_int ("Keys/RotateLeft");
    keys[K_ROTLEFT] = k ? k : defaultkeys[K_ROTLEFT];
    k = gnome_config_get_int ("Keys/Down");
    keys[K_DOWN] = k ? k : defaultkeys[K_DOWN];
    k = gnome_config_get_int ("Keys/Drop");
    keys[K_DROP] = k ? k : defaultkeys[K_DROP];
    k = gnome_config_get_int ("Keys/Discard");
    keys[K_DISCARD] = k ? k : defaultkeys[K_DISCARD];
    k = gnome_config_get_int ("Keys/Message");
    keys[K_GAMEMSG] = k ? k : defaultkeys[K_GAMEMSG];

    gnome_config_pop_prefix ();

    config_loadtheme (currenttheme);
}

void config_saveconfig (void)
{
    gnome_config_push_prefix ("/"APPID"/");

    gnome_config_set_string ("Themes/ThemeDir", currenttheme);

    gnome_config_set_string ("Sound/MidiPlayer", midicmd);
    gnome_config_set_int ("Sound/EnableSound", soundenable);
    gnome_config_set_int ("Sound/EnableMidi", midienable);

    gnome_config_set_string ("Player/Nickname", nick);
    gnome_config_set_string ("Player/Server", server);
    gnome_config_set_string ("Player/Team", team);

    gnome_config_set_int ("Keys/Right", keys[K_RIGHT]);
    gnome_config_set_int ("Keys/Left", keys[K_LEFT]);
    gnome_config_set_int ("Keys/RotateRight", keys[K_ROTRIGHT]);
    gnome_config_set_int ("Keys/RotateLeft", keys[K_ROTLEFT]);
    gnome_config_set_int ("Keys/Down", keys[K_DOWN]);
    gnome_config_set_int ("Keys/Drop", keys[K_DROP]);
    gnome_config_set_int ("Keys/Discard", keys[K_DISCARD]);
    gnome_config_set_int ("Keys/Message", keys[K_GAMEMSG]);

    gnome_config_pop_prefix ();
    gnome_config_sync ();
}
