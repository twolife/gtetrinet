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

#include "images/team24.xpm"

#define GTET_STOCK_TEAM24 "gtet-team24"

static const GtkActionEntry entries[] = {
  {"GameMenu", NULL, N_("_Game"), NULL, NULL, NULL},
  {"SettingsMenu", NULL, N_("_Settings"), NULL, NULL, NULL},
  {"HelpMenu", NULL, N_("_Help"), NULL, NULL, NULL},

  {"Connect", NULL, N_("_Connect to server..."), "<control>C", NULL, connect_command},
  {"Disconnect", NULL, N_("_Disconnect from server"), "<control>D", NULL, disconnect_command},
  {"ChangeTeam", NULL, N_("Change _team..."), "<control>T", NULL, team_command},
  {"StartGame", NULL, N_("_Start game"), "<control>N", NULL, start_command},
  {"PauseGame", NULL, N_("_Pause game"), "<control>P", NULL, pause_command},
  {"EndGame", NULL, N_("_End game"), "<control>S", NULL, end_command},
  /* Detach stuff is not ready, says Ka-shu, so make it configurable at
   * compile time for now. */
#ifdef ENABLE_DETACH
  {"DetachPage", NULL, N_("Detac_h page..."), NULL, NULL, detach_command},
#endif /* ENABLE_DETACH */
  {"Exit", GTK_STOCK_QUIT, NULL, "<control>Q", NULL, destroymain},
  {"Preferences", GTK_STOCK_PREFERENCES, NULL, NULL, NULL, preferences_command},
  {"About", GTK_STOCK_ABOUT, NULL, NULL, NULL, about_command},
};

static const char *ui_description =
"<ui>"
  "<menubar name='MainMenu'>"
    "<menu action='GameMenu'>"
      "<menuitem action='Connect' />"
      "<menuitem action='Disconnect' />"
      "<separator />"
      "<menuitem action='ChangeTeam' />"
      "<separator />"
      "<menuitem action='StartGame' />"
      "<menuitem action='PauseGame' />"
      "<menuitem action='EndGame' />"
#ifdef ENABLE_DETACH
      "<separator />"
      "<menuitem action='DetachPage' />"
#endif
      "<separator />"
      "<menuitem action='Exit' />"
    "</menu>"
    "<menu action='SettingsMenu'>"
      "<menuitem action='Preferences' />"
    "</menu>"
    "<menu action='HelpMenu'>"
      "<menuitem action='About' />"
    "</menu>"
  "</menubar>"
"</ui>";

GnomeUIInfo toolbar[] = {
    GNOMEUIINFO_ITEM_STOCK(N_("Connect"), N_("Connect to a server"), connect_command, GTK_STOCK_CONNECT),
    GNOMEUIINFO_ITEM_STOCK(N_("Disconnect"), N_("Disconnect from the current server"), disconnect_command, GTK_STOCK_DISCONNECT),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM_STOCK(N_("Start game"), N_("Start a new game"), start_command, GTK_STOCK_MEDIA_PLAY),
    GNOMEUIINFO_ITEM_STOCK(N_("End game"), N_("End the current game"), end_command, GTK_STOCK_MEDIA_STOP),
    GNOMEUIINFO_ITEM_STOCK(N_("Pause game"), N_("Pause the game"), pause_command, GTK_STOCK_MEDIA_PAUSE),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM_STOCK(N_("Change team"), N_("Change your current team name"), team_command, GTET_STOCK_TEAM24),
#ifdef ENABLE_DETACH
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM_STOCK(N_("Detach page"), N_("Detach the current notebook page"), detach_command, GTK_STOCK_CUT),
#endif
    GNOMEUIINFO_END
};

static GtkActionGroup *action_group;
#define ACTION_SHOW(name) \
  gtk_action_set_visible (gtk_action_group_get_action (action_group, name), TRUE)
#define ACTION_HIDE(name) \
  gtk_action_set_visible (gtk_action_group_get_action (action_group, name), FALSE)
#define ACTION_ENABLE(name) \
  gtk_action_set_sensitive (gtk_action_group_get_action (action_group, name), TRUE)
#define ACTION_DISABLE(name) \
  gtk_action_set_sensitive (gtk_action_group_get_action (action_group, name), FALSE)

void make_menus (GnomeApp *app)
{
  GtkIconFactory *icon_factory;
  GdkPixbuf *team24_pixbuf;
  GtkIconSet *team24_icon_set;
  GError *err = NULL;
  GtkUIManager *ui_manager;
  GtkAccelGroup *accel_group;
  GtkWidget *menubar;

  icon_factory = gtk_icon_factory_new ();
  team24_pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **)team24_xpm);
  team24_icon_set = gtk_icon_set_new_from_pixbuf (team24_pixbuf);
  g_object_unref (team24_pixbuf);
  gtk_icon_factory_add (icon_factory, GTET_STOCK_TEAM24, team24_icon_set);
  gtk_icon_set_unref (team24_icon_set);
  gtk_icon_factory_add_default (icon_factory);
  g_object_unref (icon_factory);

  action_group = gtk_action_group_new ("MenuActions");
  gtk_action_group_add_actions (action_group, entries, G_N_ELEMENTS (entries), app);

  ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

  accel_group = gtk_ui_manager_get_accel_group (ui_manager);
  gtk_window_add_accel_group (GTK_WINDOW (app), accel_group);

  if (!gtk_ui_manager_add_ui_from_string (ui_manager, ui_description, -1, &err)) {
    g_message ("building menus failed: %s", err->message);
    g_error_free (err);
    exit (EXIT_FAILURE);
  }

  menubar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");
  gnome_app_set_menus (app, GTK_MENU_BAR (menubar));

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
        ACTION_DISABLE ("Connect");
        ACTION_ENABLE ("Disconnect");

        gtk_widget_set_sensitive (toolbar[0].widget, FALSE);
        gtk_widget_set_sensitive (toolbar[1].widget, TRUE);
    }
    else {
        ACTION_ENABLE ("Connect");
        ACTION_DISABLE ("Disconnect");

        gtk_widget_set_sensitive (toolbar[0].widget, TRUE);
        gtk_widget_set_sensitive (toolbar[1].widget, FALSE);
    }
    if (moderator) {
        if (ingame) {
            ACTION_DISABLE ("StartGame");
            ACTION_ENABLE ("PauseGame");
            ACTION_ENABLE ("EndGame");

            gtk_widget_set_sensitive (toolbar[3].widget, FALSE);
            gtk_widget_set_sensitive (toolbar[4].widget, TRUE);
            gtk_widget_set_sensitive (toolbar[5].widget, TRUE);
        }
        else {
            ACTION_ENABLE ("StartGame");
            ACTION_DISABLE ("PauseGame");
            ACTION_DISABLE ("EndGame");

            gtk_widget_set_sensitive (toolbar[3].widget, TRUE);
            gtk_widget_set_sensitive (toolbar[4].widget, FALSE);
            gtk_widget_set_sensitive (toolbar[5].widget, FALSE);
        }
    }
    else {
        ACTION_DISABLE ("StartGame");
        ACTION_DISABLE ("PauseGame");
        ACTION_DISABLE ("EndGame");

        gtk_widget_set_sensitive (toolbar[3].widget, FALSE);
        gtk_widget_set_sensitive (toolbar[4].widget, FALSE);
        gtk_widget_set_sensitive (toolbar[5].widget, FALSE);
    }
    if (ingame || spectating) {
        ACTION_DISABLE ("ChangeTeam");

        gtk_widget_set_sensitive (toolbar[7].widget, FALSE);
    }
    else {
        ACTION_ENABLE ("ChangeTeam");

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
