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
#include <gconf/gconf-client.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
#include "gtetrinet.h"
#include "client.h"
#include "tetrinet.h"
#include "sound.h"
#include "misc.h"
#include "tetris.h"
#include "fields.h"

char blocksfile[1024];
int bsize;

char currenttheme[1024];

extern GConfClient *gconf_client;

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

guint defaultkeys[K_NUM] = {
    GDK_Right,
    GDK_Left,
    GDK_Up,
    GDK_Control_R,
    GDK_Down,
    GDK_space,
    GDK_d,
    GDK_t
};

guint keys[K_NUM];

/* themedir is assumed to have a trailing slash */
void config_loadtheme (const gchar *themedir)
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
    unsigned int k;
    int l;
    gchar *p;
    
    /* get the current theme */
    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/themes/theme_dir", NULL);
    /* if there is no theme configured, then we fallback to DEFAULTTHEME */
    if (strlen (p) == 0) 
    {
      g_free (p);
      p = g_strdup (DEFAULTTHEME);
    }
    GTET_O_STRCPY(currenttheme, p);
    g_free (p);
    /* add trailing slash if none exists */
    l = strlen(currenttheme);
    if (currenttheme[l-1] != '/') {
      GTET_O_STRCAT(currenttheme, "/");
    }

    /* get the midi player */
    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/sound/midi_player", NULL);
    GTET_O_STRCPY(midicmd, p);
    g_free (p);

    /* get the other sound options */
    soundenable = gconf_client_get_bool (gconf_client, "/apps/gtetrinet/sound/enable_sound", NULL);
    midienable  = gconf_client_get_bool (gconf_client, "/apps/gtetrinet/sound/enable_midi",  NULL);

    /* get the player nickname */
    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/player/nickname", NULL);
    if (p) {
        GTET_O_STRCPY(nick, p);
        g_free(p);
    }

    /* get the server name */
    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/player/server", NULL);
    if (p) {
        GTET_O_STRCPY(server, p);
        g_free(p);
    }

    /* get the team name */
    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/player/team", NULL);
    if (p) {
        GTET_O_STRCPY(team, p);
        g_free(p);
    }

    /* get the keys */
    k = gconf_client_get_int (gconf_client, "/apps/gtetrinet/keys/right", NULL);
    keys[K_RIGHT] = gdk_keyval_to_lower (k ? k : defaultkeys[K_RIGHT]);
    k = gconf_client_get_int (gconf_client, "/apps/gtetrinet/keys/left", NULL);
    keys[K_LEFT] = gdk_keyval_to_lower (k ? k : defaultkeys[K_LEFT]);
    k = gconf_client_get_int (gconf_client, "/apps/gtetrinet/keys/rotate_right", NULL);
    keys[K_ROTRIGHT] = gdk_keyval_to_lower (k ? k : defaultkeys[K_ROTRIGHT]);
    k = gconf_client_get_int (gconf_client, "/apps/gtetrinet/keys/rotate_left", NULL);
    keys[K_ROTLEFT] = gdk_keyval_to_lower (k ? k : defaultkeys[K_ROTLEFT]);
    k = gconf_client_get_int (gconf_client, "/apps/gtetrinet/keys/down", NULL);
    keys[K_DOWN] = gdk_keyval_to_lower (k ? k : defaultkeys[K_DOWN]);
    k = gconf_client_get_int (gconf_client, "/apps/gtetrinet/keys/drop", NULL);
    keys[K_DROP] = gdk_keyval_to_lower (k ? k : defaultkeys[K_DROP]);
    k = gconf_client_get_int (gconf_client, "/apps/gtetrinet/keys/discard", NULL);
    keys[K_DISCARD] = gdk_keyval_to_lower (k ? k : defaultkeys[K_DISCARD]);
    k = gconf_client_get_int (gconf_client, "/apps/gtetrinet/keys/message", NULL);
    keys[K_GAMEMSG] = gdk_keyval_to_lower (k ? k : defaultkeys[K_GAMEMSG]);

    config_loadtheme (currenttheme);
}

void load_theme (const gchar *theme_dir)
{
  /* load the theme */
  GTET_O_STRCPY (currenttheme, theme_dir);
  config_loadtheme (theme_dir);

  /* update the fields */
  fields_page_destroy_contents ();
  fields_cleanup ();
  fields_init ();
  fields_page_new ();
  fieldslabelupdate();
  if (ingame)
  {
    sound_stopmidi ();
    sound_playmidi (midifile);
    tetrinet_redrawfields ();
  }
}

void
sound_midi_player_changed (GConfClient *client,
                           guint cnxn_id,
                           GConfEntry *entry,
                           gpointer user_data)
{
  GTET_O_STRCPY (midicmd, gconf_value_get_string (gconf_entry_get_value (entry)));
  if (ingame)
  {
    sound_stopmidi ();
    sound_playmidi (midifile);
  }
}

void
sound_enable_sound_changed (GConfClient *client,
                            guint cnxn_id,
                            GConfEntry *entry,
                            gpointer user_data)
{
  soundenable = gconf_value_get_bool (gconf_entry_get_value (entry));
  if (!soundenable)
    gconf_client_set_bool (gconf_client, "/apps/gtetrinet/sound/enable_midi", FALSE, NULL);
}

void
sound_enable_midi_changed (GConfClient *client,
                           guint cnxn_id,
                           GConfEntry *entry,
                           gpointer user_data)
{
  midienable = gconf_value_get_bool (gconf_entry_get_value (entry));
  if (!midienable)
    sound_stopmidi ();
}

void
themes_theme_dir_changed (GConfClient *client,
                          guint cnxn_id,
                          GConfEntry *entry,
                          gpointer user_data)
{
  load_theme (gconf_value_get_string (gconf_entry_get_value (entry)));
}
