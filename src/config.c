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
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "gtet_config.h"
#include "gtetrinet.h"
#include "client.h"
#include "tetrinet.h"
#include "sound.h"
#include "misc.h"
#include "tetris.h"
#include "fields.h"
#include "partyline.h"

char blocksfile[1024];
int bsize;

GString *currenttheme = NULL;

static char *soundkeys[S_NUM] = {
    "Drop",
    "Solidify",
    "LineClear",
    "Tetris",
    "Rotate",
    "SpecialLine",
    "YouWin",
    "YouLose",
    "Message",
    "GameStart",
};

guint defaultkeys[K_NUM] = {
    GDK_KEY_Right,
    GDK_KEY_Left,
    GDK_KEY_Up,
    GDK_KEY_Control_R,
    GDK_KEY_Down,
    GDK_KEY_space,
    GDK_KEY_d,
    GDK_KEY_t,
    GDK_KEY_1,
    GDK_KEY_2,
    GDK_KEY_3,
    GDK_KEY_4,
    GDK_KEY_5,
    GDK_KEY_6
};

guint keys[K_NUM];

void load_theme (const gchar *theme_dir)
{
  /* load the theme */
  g_string_assign(currenttheme, theme_dir);
  config_loadtheme (theme_dir);

  /* update the fields */
  fields_page_destroy_contents ();
  fields_cleanup ();
  fields_init ();
  fields_page_new ();
  fieldslabelupdate();
  if (ingame)
  {
    tetrinet_redrawfields ();
  }
}

/* themedir is assumed to have a trailing slash */
void config_loadtheme (const gchar *themedir)
{
    char buf[1024], *p;
    int i;
    GKeyFile *keyfile;
    GError *err = NULL;

    GTET_O_STRCPY(buf, themedir);
    GTET_O_STRCAT(buf, "theme.cfg");

    keyfile = g_key_file_new ();
    if (!g_key_file_load_from_file (keyfile, buf, 0, &err))
      goto bad_theme;

    p = g_key_file_get_string (keyfile, "Theme", "Name", &err);
    if (!p)
      goto bad_theme;
    g_free (p);

    p = g_key_file_get_string (keyfile, "Graphics", "Blocks", NULL);
    if (!p)
      p = g_strdup("blocks.png");
    
    GTET_O_STRCPY(blocksfile, themedir);
    GTET_O_STRCAT(blocksfile, p);
    g_free (p);
    bsize = g_key_file_get_integer (keyfile, "Graphics", "BlockSize", &err);
    if (err)
    {
      bsize = 16;
      g_error_free (err);
      err = NULL;
    }

    for (i = 0; i < S_NUM; i ++) {
        p = g_key_file_get_string (keyfile, "Sounds", soundkeys[i], NULL);
        if (p) {
            GTET_O_STRCPY(soundfiles[i], themedir);
            GTET_O_STRCAT(soundfiles[i], p);
            g_free (p);
        }
        else
            soundfiles[i][0] = 0;
    }

    sound_cache ();

    g_key_file_unref (keyfile);

    return;

 bad_theme:
    {
      GtkWidget *mb;
      mb = gtk_message_dialog_new (NULL,
                                   0,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_OK,
                                   _("Warning: theme does not have a name, "
                                     "reverting to default."));
      gtk_dialog_run (GTK_DIALOG (mb));
      gtk_widget_destroy (mb);
      g_key_file_unref (keyfile);
      g_string_assign(currenttheme, DEFAULTTHEME);
      config_loadtheme (currenttheme->str);
    }
}

/* Arrggh... all these params are sizeof() == 1024 ... this needs a real fix */
int config_getthemeinfo (char *themedir, char *name, char *author, char *desc)
{
    char buf[1024];
    char *p = NULL;
    GKeyFile *keyfile;

    GTET_O_STRCPY (buf, themedir);
    GTET_O_STRCAT (buf, "theme.cfg");

    keyfile = g_key_file_new ();
    if (!g_key_file_load_from_file (keyfile, buf, 0, NULL)) {
        g_key_file_unref (keyfile);
        return -1;
    }

    p = g_key_file_get_string (keyfile, "Theme", "Name", NULL);
    if (p == 0) {
        g_key_file_unref (keyfile);
        return -1;
    }
    else {
        if (name) GTET_STRCPY(name, p, 1024);
        g_free (p);
    }
    if (author) {
        if ((p = g_key_file_get_string (keyfile, "Theme", "Author", NULL))) {
            GTET_STRCPY(author, p, 1024);
            g_free (p);
        } else {
            author[0] = 0;
        }
    }
    if (desc) {
        if ((p = g_key_file_get_string (keyfile, "Theme", "Description", NULL))) {
            GTET_STRCPY(desc, p, 1024);
            g_free (p);
        } else {
            desc[0] = 0;
        }
    }

    g_key_file_unref (keyfile);

    return 0;
}

void config_loadconfig (void)
{
    gchar *p;

    /* get the other sound options */
    soundenable = g_settings_get_boolean (settings, "sound-enable");
    if (soundenable) {
        sound_cache ();
    }

    /* get the player nickname */
    p = g_settings_get_string (settings, "player-nickname");
    if (p) {
        GTET_O_STRCPY(nick, p);
        g_free(p);
    }

    /* get the server name */
    p = g_settings_get_string (settings, "server");
    if (p) {
        GTET_O_STRCPY(server, p);
        g_free(p);
    }

    /* get the team name */
    p = g_settings_get_string (settings, "player-team");
    if (p) {
        GTET_O_STRCPY(team, p);
        g_free(p);
    }
	 
    /* get the game mode */
    gamemode = g_settings_get_boolean (settings, "gamemode");

    /* Get the timestamp option. */
    timestampsenable = g_settings_get_boolean (settings, "partyline-enable-timestamps");

    /* Get the channel list option */
    list_enabled = g_settings_get_boolean (settings, "partyline-enable-channel-list");

    partyline_show_channel_list(list_enabled);
}

void config_loadconfig_themes (void)
{
    gchar *p;

    currenttheme = g_string_sized_new(100);
    
    /* get the current theme */
    p = g_settings_get_string (settings_themes, "directory");
    /* if there is no theme configured, then we fallback to DEFAULTTHEME */
    if (!p || !p[0])
    {
      g_string_assign(currenttheme, DEFAULTTHEME);
      g_settings_set_string (settings_themes, "directory", currenttheme->str);
    }
    else
      g_string_assign(currenttheme, p);
    g_free (p);
    
    /* add trailing slash if none exists */
    if (currenttheme->str[currenttheme->len - 1] != '/')
      g_string_append_c(currenttheme, '/');

    load_theme (currenttheme->str);
}

void config_loadconfig_keys (void)
{
    gchar *p;

    /* get the keys */
    p = g_settings_get_string (settings_keys, "right");
    if (p)
    {
      keys[K_RIGHT] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_RIGHT] = defaultkeys[K_RIGHT];
    
    p = g_settings_get_string (settings_keys, "left");
    if (p)
    {
      keys[K_LEFT] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_LEFT] = defaultkeys[K_LEFT];

    p = g_settings_get_string (settings_keys, "rotate-right");
    if (p)
    {
      keys[K_ROTRIGHT] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_ROTRIGHT] = defaultkeys[K_ROTRIGHT];

    p = g_settings_get_string (settings_keys, "rotate-left");
    if (p)
    {
      keys[K_ROTLEFT] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_ROTLEFT] = defaultkeys[K_ROTLEFT];

    p = g_settings_get_string (settings_keys, "down");
    if (p)
    {
      keys[K_DOWN] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_DOWN] = defaultkeys[K_DOWN];

    p = g_settings_get_string (settings_keys, "drop");
    if (p)
    {
      keys[K_DROP] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_DROP] = defaultkeys[K_DROP];

    p = g_settings_get_string (settings_keys, "discard");
    if (p)
    {
      keys[K_DISCARD] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_DISCARD] = defaultkeys[K_DISCARD];

    p = g_settings_get_string (settings_keys, "message");
    if (p)
    {
      keys[K_GAMEMSG] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_GAMEMSG] = defaultkeys[K_GAMEMSG];

    p = g_settings_get_string (settings_keys, "special1");
    if (p)
    {
      keys[K_SPECIAL1] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_SPECIAL1] = defaultkeys[K_SPECIAL1];

    p = g_settings_get_string (settings_keys, "special2");
    if (p)
    {
      keys[K_SPECIAL2] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_SPECIAL2] = defaultkeys[K_SPECIAL2];

    p = g_settings_get_string (settings_keys, "special3");
    if (p)
    {
      keys[K_SPECIAL3] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_SPECIAL3] = defaultkeys[K_SPECIAL3];

    p = g_settings_get_string (settings_keys, "special4");
    if (p)
    {
      keys[K_SPECIAL4] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_SPECIAL4] = defaultkeys[K_SPECIAL4];

    p = g_settings_get_string (settings_keys, "special5");
    if (p)
    {
      keys[K_SPECIAL5] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_SPECIAL5] = defaultkeys[K_SPECIAL5];

    p = g_settings_get_string (settings_keys, "special6");
    if (p)
    {
      keys[K_SPECIAL6] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_SPECIAL6] = defaultkeys[K_SPECIAL6];
}

