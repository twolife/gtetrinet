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
    GNOMEUIINFO_ITEM(N_("_Connect to Server..."), NULL, connect_command, NULL),
    GNOMEUIINFO_ITEM(N_("_Disconnect from Server"), NULL, disconnect_command, NULL),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM(N_("Change _Team..."), NULL, team_command, NULL),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM(N_("_Start game"), NULL, start_command, NULL),
    GNOMEUIINFO_ITEM(N_("_Pause game"), NULL, pause_command, NULL),
    GNOMEUIINFO_ITEM(N_("_End game"), NULL, end_command, NULL),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM(N_("Detac_h Page..."), NULL, detach_command, NULL),
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
    GNOMEUIINFO_ITEM(N_("Connect"), NULL, connect_command, NULL),
    GNOMEUIINFO_ITEM(N_("Disconnect"), NULL, disconnect_command, NULL),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM(N_("Start game"), NULL, start_command, NULL),
    GNOMEUIINFO_ITEM(N_("Pause game"), NULL, pause_command, NULL),
    GNOMEUIINFO_ITEM(N_("End game"), NULL, end_command, NULL),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM(N_("Change team"), NULL, team_command, NULL),\
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM(N_("Detach Page"), NULL, detach_command, NULL),
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

void detach_command (GtkWidget *widget, gpointer data)
{
    move_current_page_to_window ();
}

void start_command (GtkWidget *widget, gpointer data)
{
    char buf[16];
    sprintf (buf, "%i %i", 1, playernum);
    client_outmessage (OUT_STARTGAME, buf);
}

void end_command (GtkWidget *widget, gpointer data)
{
    char buf[16];
    sprintf (buf, "%i %i", 0, playernum);
    client_outmessage (OUT_STARTGAME, buf);
}

void pause_command (GtkWidget *widget, gpointer data)
{
    char buf[16];
    sprintf (buf, "%i %i", paused?0:1, playernum);
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
        strcpy (buf, _("Connected to\n"));
        strcat (buf, server);
        partyline_status (buf);
    }
    else partyline_status (_("Not connected"));
}


/* about... */

void about_command (GtkWidget *widget, gpointer data)
{
    GtkWidget *about;

    const char *authors[] = {N_("Ka-shu Wong <kswong@zip.com.au>"), NULL};
    
    about = gnome_about_new (APPNAME, APPVERSION,
                             _("(C) 1999, 2000, 2001, 2002 Ka-shu Wong"),
                             authors,
                             _("A Tetrinet client for GNOME.\n"
                               "Homepage: http://gtetrinet.sourceforge.net/\n"),
                             NULL);
    gtk_widget_show (about);
}
