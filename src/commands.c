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
#include <string.h>
#include <stdio.h>

#include "gtetrinet.h"
#include "client.h"
#include "tetrinet.h"
#include "partyline.h"
#include "misc.h"
#include "commands.h"
#include "dialogs.h"

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
    GNOMEUIINFO_ITEM_STOCK(N_("Connect"), N_("Connect"), connect_command, "gtk-execute"),
    GNOMEUIINFO_ITEM_STOCK(N_("Disconnect"), N_("Disconnect"), disconnect_command, "gtk-quit"),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM_STOCK(N_("Start game"), N_("Start game"), start_command, "gtk-go-forward"),
    GNOMEUIINFO_ITEM_STOCK(N_("Pause game"), N_("Pause game"), pause_command, "gtk-dialog-warning"),
    GNOMEUIINFO_ITEM_STOCK(N_("End game"), N_("End game"), end_command, "gtk-stop"),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM_STOCK(N_("Change team"), N_("Change team"), team_command, "gtk-jump-to"),
#ifdef ENABLE_DETACH
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM(N_("Detach page"), NULL, detach_command, NULL),
#endif
    GNOMEUIINFO_END
};

void make_menus (GnomeApp *app)
{
    gnome_app_create_menus (app, menubar);
    gnome_app_create_toolbar (app, toolbar);
}

/* callbacks */

void connect_command (GtkWidget *widget, gpointer data)
{
    connectdialog_new ();
}

void disconnect_command (GtkWidget *widget, gpointer data)
{
    client_destroy ();
}

void team_command (GtkWidget *widget, gpointer data)
{
    teamdialog_new ();
}

#ifdef ENABLE_DETACH
void detach_command (GtkWidget *widget, gpointer data)
{
    move_current_page_to_window ();
}
#endif

void start_command (GtkWidget *widget, gpointer data)
{
    char buf[22];
    g_snprintf (buf, sizeof(buf), "%i %i", 1, playernum);
    client_outmessage (OUT_STARTGAME, buf);
}

void end_command (GtkWidget *widget, gpointer data)
{
    char buf[22];
    g_snprintf (buf, sizeof(buf), "%i %i", 0, playernum);
    client_outmessage (OUT_STARTGAME, buf);
}

void pause_command (GtkWidget *widget, gpointer data)
{
    char buf[22];
    g_snprintf (buf, sizeof(buf), "%i %i", paused?0:1, playernum);
    client_outmessage (OUT_PAUSE, buf);
}

void preferences_command (GtkWidget *widget, gpointer data)
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


/* about... */

void about_command (GtkWidget *widget, gpointer data)
{
    GtkWidget *hbox;
    GdkPixbuf *logo;
    static GtkWidget *about = NULL;

    if (!GTK_IS_WINDOW (about))
    {
      const char *authors[] = {N_("Ka-shu Wong <kswong@zip.com.au>"),
                               N_("James Antill <james@and.org>"),
			       N_("Jordi Mallach <jordi@sindominio.net>"),
			       N_("Dani Carbonell <bocata@panete.net>"),
                               NULL};
      const char *documenters[] = {N_("Jordi Mallach <jordi@sindominio.net>"),
                                   NULL};
      /* Translators: translate as your names & emails */
      const char *translators = _("translator_credits");

      logo = gdk_pixbuf_new_from_file (PIXMAPSDIR "/gtetrinet.png", NULL);
    
      about = gnome_about_new (APPNAME, APPVERSION,
                               _("(C) 1999, 2000, 2001, 2002, 2003 Ka-shu Wong"),
                               _("A Tetrinet client for GNOME.\n"),
                               authors,
                               documenters,
			      strcmp (translators, "translator_credits") != 0 ?
				       translators : NULL,
			      logo);

      if (logo != NULL)
		  g_object_unref (logo);

      hbox = gtk_hbox_new (TRUE, 0);
      gtk_box_pack_start (GTK_BOX (hbox),
		  gnome_href_new ("http://gtetrinet.sourceforge.net/", _("GTetrinet Home Page")),
		  FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (about)->vbox),
		          hbox, TRUE, FALSE, 0);
      gtk_widget_show_all (hbox);

      g_signal_connect(G_OBJECT(about), "destroy",
		       G_CALLBACK(gtk_widget_destroyed), &about);

      gtk_widget_show (about);
    }
    else
    {
      gtk_window_present (GTK_WINDOW (about));
    }
}
