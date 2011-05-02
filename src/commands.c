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
#include <gnome.h>
#include <string.h>
#include <stdio.h>

#include "gtetrinet.h"
#include "client.h"
#include "tetrinet.h"
#include "partyline.h"
#include "misc.h"
#include "commands.h"
#include "dialogs.h"

#include "images/play.xpm"
#include "images/pause.xpm"
#include "images/stop.xpm"
#include "images/team24.xpm"
#include "images/connect.xpm"
#include "images/disconnect.xpm"

GnomeUIInfo gamemenu[] = {
    GNOMEUIINFO_ITEM(N_("_Connect to server..."), NULL, connect_command, NULL),
    GNOMEUIINFO_ITEM(N_("_Disconnect from server"), NULL, disconnect_command, NULL),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM(N_("Change _team..."), NULL, team_command, NULL),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM(N_("_Start game"), NULL, start_command, NULL),
    GNOMEUIINFO_ITEM(N_("_Pause game"), NULL, pause_command, NULL),
    GNOMEUIINFO_ITEM(N_("_End game"), NULL, end_command, NULL),
    /* Detach stuff is not ready, says Ka-shu, so make it configurable at
     * compile time for now. */
#ifdef ENABLE_DETACH
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM(N_("Detac_h page..."), NULL, detach_command, NULL),
#endif /* ENABLE_DETACH */
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_MENU_EXIT_ITEM(destroymain, NULL),
    GNOMEUIINFO_END
};

GnomeUIInfo settingsmenu[] = {
    GNOMEUIINFO_MENU_PREFERENCES_ITEM(preferences_command, NULL),
    GNOMEUIINFO_END
};

GnomeUIInfo helpmenu[] = {
    GNOMEUIINFO_MENU_ABOUT_ITEM(about_command, NULL),
    GNOMEUIINFO_END
};

GnomeUIInfo menubar[] = {
    GNOMEUIINFO_MENU_GAME_TREE(gamemenu),
    GNOMEUIINFO_MENU_SETTINGS_TREE(settingsmenu),
    GNOMEUIINFO_MENU_HELP_TREE(helpmenu),
    GNOMEUIINFO_END
};

GnomeUIInfo toolbar[] = {
    GNOMEUIINFO_ITEM_DATA(N_("Connect"), N_("Connect to a server"), connect_command, NULL, connect_xpm),
    GNOMEUIINFO_ITEM_DATA(N_("Disconnect"), N_("Disconnect from the current server"), disconnect_command, NULL, disconnect_xpm),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM_DATA(N_("Start game"), N_("Start a new game"), start_command, NULL, play_xpm),
    GNOMEUIINFO_ITEM_DATA(N_("End game"), N_("End the current game"), end_command, NULL, stop_xpm),
    GNOMEUIINFO_ITEM_DATA(N_("Pause game"), N_("Pause the game"), pause_command, NULL, pause_xpm),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM_DATA(N_("Change team"), N_("Change your current team name"), team_command, NULL, team24_xpm),
#ifdef ENABLE_DETACH
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM_STOCK(N_("Detach page"), N_("Detach the current notebook page"), detach_command, "gtk-cut"),
#endif
    GNOMEUIINFO_END
};

void make_menus (GnomeApp *app)
{
  gnome_app_create_menus (app, menubar);

  gtk_accel_map_add_entry ("<GTetrinet-Main>/Game/Start", gdk_keyval_from_name ("n"), GDK_CONTROL_MASK);
  gtk_accel_map_add_entry ("<GTetrinet-Main>/Game/Pause", gdk_keyval_from_name ("p"), GDK_CONTROL_MASK);
  gtk_accel_map_add_entry ("<GTetrinet-Main>/Game/Stop", gdk_keyval_from_name ("s"), GDK_CONTROL_MASK);
  gtk_accel_map_add_entry ("<GTetrinet-Main>/Game/Connect", gdk_keyval_from_name ("c"), GDK_CONTROL_MASK);
  gtk_accel_map_add_entry ("<GTetrinet-Main>/Game/Disconnect", gdk_keyval_from_name ("d"), GDK_CONTROL_MASK);
  gtk_accel_map_add_entry ("<GTetrinet-Main>/Game/Change_Team", gdk_keyval_from_name ("t"), GDK_CONTROL_MASK);
  
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM (gamemenu[0].widget), "<GTetrinet-Main>/Game/Connect");
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM (gamemenu[1].widget), "<GTetrinet-Main>/Game/Disconnect");
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM (gamemenu[3].widget), "<GTetrinet-Main>/Game/Change_Team");
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM (gamemenu[5].widget), "<GTetrinet-Main>/Game/Start");
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM (gamemenu[6].widget), "<GTetrinet-Main>/Game/Pause");
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM (gamemenu[7].widget), "<GTetrinet-Main>/Game/Stop");

  gnome_app_create_toolbar (app, toolbar);
  gtk_widget_hide (toolbar[4].widget);
  gtk_widget_hide (toolbar[1].widget);
}

/* callbacks */

void connect_command (void)
{
    connectdialog_new ();
}

void disconnect_command (void)
{
    client_disconnect ();
}

void team_command (void)
{
    teamdialog_new ();
}

#ifdef ENABLE_DETACH
void detach_command (void)
{
    move_current_page_to_window ();
}
#endif

void start_command (void)
{
  char buf[22];
  
  g_snprintf (buf, sizeof(buf), "%i %i", 1, playernum);
  client_outmessage (OUT_STARTGAME, buf);
}

void show_connect_button (void)
{
  gtk_widget_hide (toolbar[1].widget);
  gtk_widget_show (toolbar[0].widget);
}

void show_disconnect_button (void)
{
  gtk_widget_hide (toolbar[0].widget);
  gtk_widget_show (toolbar[1].widget);
}

void show_stop_button (void)
{
  gtk_widget_hide (toolbar[3].widget);
  gtk_widget_show (toolbar[4].widget);
}

void show_start_button (void)
{
  gtk_widget_hide (toolbar[4].widget);
  gtk_widget_show (toolbar[3].widget);
}

void end_command (void)
{
  char buf[22];
  
  g_snprintf (buf, sizeof(buf), "%i %i", 0, playernum);
  client_outmessage (OUT_STARTGAME, buf);
}

void pause_command (void)
{
  char buf[22];
  
  g_snprintf (buf, sizeof(buf), "%i %i", paused?0:1, playernum);
  client_outmessage (OUT_PAUSE, buf);
}

void preferences_command (void)
{
    prefdialog_new ();
}


/* the following function enable/disable things */

void commands_checkstate ()
{
    if (connected) {
        gtk_widget_set_sensitive (gamemenu[0].widget, FALSE);
        gtk_widget_set_sensitive (gamemenu[1].widget, TRUE);

        gtk_widget_set_sensitive (toolbar[0].widget, FALSE);
        gtk_widget_set_sensitive (toolbar[1].widget, TRUE);
    }
    else {
        gtk_widget_set_sensitive (gamemenu[0].widget, TRUE);
        gtk_widget_set_sensitive (gamemenu[1].widget, FALSE);

        gtk_widget_set_sensitive (toolbar[0].widget, TRUE);
        gtk_widget_set_sensitive (toolbar[1].widget, FALSE);
    }
    if (moderator) {
        if (ingame) {
            gtk_widget_set_sensitive (gamemenu[5].widget, FALSE);
            gtk_widget_set_sensitive (gamemenu[6].widget, TRUE);
            gtk_widget_set_sensitive (gamemenu[7].widget, TRUE);

            gtk_widget_set_sensitive (toolbar[3].widget, FALSE);
            gtk_widget_set_sensitive (toolbar[4].widget, TRUE);
            gtk_widget_set_sensitive (toolbar[5].widget, TRUE);
        }
        else {
            gtk_widget_set_sensitive (gamemenu[5].widget, TRUE);
            gtk_widget_set_sensitive (gamemenu[6].widget, FALSE);
            gtk_widget_set_sensitive (gamemenu[7].widget, FALSE);

            gtk_widget_set_sensitive (toolbar[3].widget, TRUE);
            gtk_widget_set_sensitive (toolbar[4].widget, FALSE);
            gtk_widget_set_sensitive (toolbar[5].widget, FALSE);
        }
    }
    else {
        gtk_widget_set_sensitive (gamemenu[5].widget, FALSE);
        gtk_widget_set_sensitive (gamemenu[6].widget, FALSE);
        gtk_widget_set_sensitive (gamemenu[7].widget, FALSE);

        gtk_widget_set_sensitive (toolbar[3].widget, FALSE);
        gtk_widget_set_sensitive (toolbar[4].widget, FALSE);
        gtk_widget_set_sensitive (toolbar[5].widget, FALSE);
    }
    if (ingame || spectating) {
        gtk_widget_set_sensitive (gamemenu[3].widget, FALSE);

        gtk_widget_set_sensitive (toolbar[7].widget, FALSE);
    }
    else {
        gtk_widget_set_sensitive (gamemenu[3].widget, TRUE);

        gtk_widget_set_sensitive (toolbar[7].widget, TRUE);
    }

    partyline_connectstatus (connected);

    if (ingame) partyline_status (_("Game in progress"));
    else if (connected) {
        char buf[256];
        GTET_O_STRCPY(buf, _("Connected to\n"));
        GTET_O_STRCAT(buf, server);
        partyline_status (buf);
    }
    else partyline_status (_("Not connected"));
}

enum {
	LINK_TYPE_EMAIL,
	LINK_TYPE_URL
};
/* handle the links */

void handle_links (GtkAboutDialog *about G_GNUC_UNUSED, const gchar *link, gpointer data)
{
    gchar *new_link;

    switch (GPOINTER_TO_INT (data)) {
    case LINK_TYPE_EMAIL:
	    new_link = g_strdup_printf ("mailto: %s", link);
	    break;
    case LINK_TYPE_URL:
	    new_link = g_strdup (link);
	    break;
    default:
	    g_assert_not_reached ();
    }

    if (!gnome_url_show (new_link, NULL)){
	    g_warning ("Unable to follow link %s\n", link);
    }
    
    g_free (new_link);
}
/* about... */

void about_command (void)
{
    GdkPixbuf *logo;
  
    const char *authors[] = {"Ka-shu Wong <kswong@zip.com.au>",
			     "James Antill <james@and.org>",
			     "Jordi Mallach <jordi@sindominio.net>",
			     "Dani Carbonell <bocata@panete.net>",
			     NULL};
    const char *documenters[] = {"Jordi Mallach <jordi@sindominio.net>",
				 NULL};
    /* Translators: translate as your names & emails */
    const char *translators = _("translator-credits");

    logo = gdk_pixbuf_new_from_file (PIXMAPSDIR "/gtetrinet.png", NULL);
    
    gtk_about_dialog_set_email_hook ((GtkAboutDialogActivateLinkFunc) handle_links,
				     GINT_TO_POINTER (LINK_TYPE_EMAIL), NULL);
    
    gtk_about_dialog_set_url_hook ((GtkAboutDialogActivateLinkFunc) handle_links,
				   GINT_TO_POINTER (LINK_TYPE_URL), NULL);
  
    gtk_show_about_dialog (NULL,
			   "name", APPNAME, 
			   "version", APPVERSION,
			   "copyright", "Copyright \xc2\xa9 2004, 2005 Jordi Mallach, Dani Carbonell\nCopyright \xc2\xa9 1999, 2000, 2001, 2002, 2003 Ka-shu Wong",
			   "comments", _("A Tetrinet client for GNOME.\n"),
			   "authors", authors,
			   "documenters", documenters,
			   "translator-credits", strcmp (translators, "translator-credits") != 0 ? translators : NULL,
			   "logo", logo,
			   "website", "http://gtetrinet.sf.net",
			   "website-label", "GTetrinet Home Page",
			   NULL);
    
    if (logo != NULL)
	    g_object_unref (logo);
}
