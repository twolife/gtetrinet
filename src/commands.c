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
#include <glib/gi18n.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

  {"Connect", GTK_STOCK_CONNECT, N_("_Connect to server..."), "<control>C", N_("Connect to a server"), connect_command},
  {"Disconnect", GTK_STOCK_DISCONNECT, N_("_Disconnect from server"), "<control>D", N_("Disconnect from the current server"), disconnect_command},
  {"ChangeTeam", GTET_STOCK_TEAM24, N_("Change _team..."), "<control>T", N_("Change your current team name"), team_command},
  {"StartGame", GTK_STOCK_MEDIA_PLAY, N_("_Start game"), "<control>N", N_("Start a new game"), start_command},
  {"PauseGame", GTK_STOCK_MEDIA_PAUSE, N_("_Pause game"), "<control>P", N_("Pause the game"), pause_command},
  {"EndGame", GTK_STOCK_MEDIA_STOP, N_("_End game"), "<control>S", N_("End the current game"), end_command},
  /* Detach stuff is not ready, says Ka-shu, so make it configurable at
   * compile time for now. */
#ifdef ENABLE_DETACH
  {"DetachPage", GTK_STOCK_CUT, N_("Detac_h page..."), "", N_("Detach the current notebook page"), detach_command},
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
  "<toolbar name='MainToolbar'>"
    "<toolitem action='Connect' />"
    "<toolitem action='Disconnect' />"
    "<separator />"
    "<toolitem action='StartGame' />"
    "<toolitem action='EndGame' />"
    "<toolitem action='PauseGame' />"
    "<separator />"
    "<toolitem action='ChangeTeam' />"
#ifdef ENABLE_DETACH
    "<separator />"
    "<toolitem action='DetachPage' />"
#endif
  "</toolbar>"
"</ui>";

static const struct {
  const char *k;
  const char *v;
} toolbar_labels[] = {
  {"Connect", N_("Connect")},
  {"Disconnect", N_("Disconnect")},
  {"StartGame", N_("Start game")},
  {"EndGame", N_("End game")},
  {"PauseGame", N_("Pause game")},
  {"ChangeTeam", N_("Change team")},
#ifdef ENABLE_DETACH
  {"DetachPage", N_("Detach page")},
#endif
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

static void fixup_toolbar_buttons (GtkUIManager *uim G_GNUC_UNUSED, GtkAction *action, GtkWidget *proxy, gpointer user_data G_GNUC_UNUSED)
{
  gsize i;

  if (GTK_IS_TOOL_BUTTON (proxy)) {
    for (i = 0; i < G_N_ELEMENTS (toolbar_labels); i++) {
      if (strcmp (toolbar_labels[i].k, gtk_action_get_name (action)) == 0) {
        gtk_tool_button_set_label (GTK_TOOL_BUTTON (proxy), _(toolbar_labels[i].v));
        gtk_tool_item_set_is_important (GTK_TOOL_ITEM (proxy), TRUE);
      }
    }
  }
}

void make_menus (GtkWindow *app)
{
  GtkIconFactory *icon_factory;
  GdkPixbuf *team24_pixbuf;
  GtkIconSet *team24_icon_set;
  GError *err = NULL;
  GtkUIManager *ui_manager;
  GtkWidget *vbox;
  GtkAccelGroup *accel_group;
  GtkWidget *menubar;
  GtkWidget *toolbar;
  GtkWidget *main_widget;

  icon_factory = gtk_icon_factory_new ();
  team24_pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **)team24_xpm);
  team24_icon_set = gtk_icon_set_new_from_pixbuf (team24_pixbuf);
  g_object_unref (team24_pixbuf);
  gtk_icon_factory_add (icon_factory, GTET_STOCK_TEAM24, team24_icon_set);
  gtk_icon_set_unref (team24_icon_set);
  gtk_icon_factory_add_default (icon_factory);
  g_object_unref (icon_factory);

  vbox = gtk_vbox_new (FALSE, 0);

  action_group = gtk_action_group_new ("MenuActions");
  gtk_action_group_add_actions (action_group, entries, G_N_ELEMENTS (entries), app);

  ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

  accel_group = gtk_ui_manager_get_accel_group (ui_manager);
  gtk_window_add_accel_group (GTK_WINDOW (app), accel_group);

  g_signal_connect (ui_manager, "connect-proxy", G_CALLBACK (fixup_toolbar_buttons), NULL);

  if (!gtk_ui_manager_add_ui_from_string (ui_manager, ui_description, -1, &err)) {
    g_message ("building menus failed: %s", err->message);
    g_error_free (err);
    exit (EXIT_FAILURE);
  }

  menubar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");
  gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, FALSE, 0);

  toolbar = gtk_ui_manager_get_widget (ui_manager, "/MainToolbar");
  gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_BOTH_HORIZ);
  gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);

  gtk_widget_show_all (GTK_WIDGET (vbox));

  main_widget = gtk_bin_get_child (GTK_BIN (app));
  if (main_widget != NULL)
  {
    g_object_ref (main_widget);
    gtk_container_remove (GTK_CONTAINER (app), main_widget);
    gtk_box_pack_start (GTK_BOX (vbox), main_widget, TRUE, TRUE, 0);
    g_object_unref (main_widget);
    gtk_container_add (GTK_CONTAINER (app), vbox);
  }

  ACTION_HIDE ("EndGame");
  ACTION_HIDE ("Disconnect");
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
  ACTION_HIDE ("Disconnect");
  ACTION_SHOW ("Connect");
}

void show_disconnect_button (void)
{
  ACTION_HIDE ("Connect");
  ACTION_SHOW ("Disconnect");
}

void show_stop_button (void)
{
  ACTION_HIDE ("StartGame");
  ACTION_SHOW ("EndGame");
}

void show_start_button (void)
{
  ACTION_HIDE ("EndGame");
  ACTION_SHOW ("StartGame");
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
    }
    else {
        ACTION_ENABLE ("Connect");
        ACTION_DISABLE ("Disconnect");
    }
    if (moderator) {
        if (ingame) {
            ACTION_DISABLE ("StartGame");
            ACTION_ENABLE ("PauseGame");
            ACTION_ENABLE ("EndGame");
        }
        else {
            ACTION_ENABLE ("StartGame");
            ACTION_DISABLE ("PauseGame");
            ACTION_DISABLE ("EndGame");
        }
    }
    else {
        ACTION_DISABLE ("StartGame");
        ACTION_DISABLE ("PauseGame");
        ACTION_DISABLE ("EndGame");
    }
    if (ingame || spectating) {
        ACTION_DISABLE ("ChangeTeam");
    }
    else {
        ACTION_ENABLE ("ChangeTeam");
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
