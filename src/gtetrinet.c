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
#include <gdk_imlib.h>
#include <locale.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>

#include "gtetrinet.h"
#include "config.h"
#include "client.h"
#include "tetrinet.h"
#include "tetris.h"
#include "fields.h"
#include "partyline.h"
#include "winlist.h"
#include "misc.h"
#include "commands.h"
#include "sound.h"

#include "images/fields.xpm"
#include "images/partyline.xpm"
#include "images/winlist.xpm"

static GtkWidget *pixmapdata_label (char **d, char *str);
static int gtetrinet_key (int keyval, int mod);
gint keypress (GtkWidget *widget, GdkEventKey *key);
gint keyrelease (GtkWidget *widget, GdkEventKey *key);

static GtkWidget *app, *pfields, *pparty, *pwinlist;
static GtkWidget *winlistwidget, *partywidget, *fieldswidget;
GtkWidget *notebook;

char *option_connect = 0, *option_nick = 0, *option_team = 0, *option_pass = 0;
int option_spec = 0;

int gamemode = ORIGINAL;

int fields_width, fields_height;

static const struct poptOption options[] = {
    {"connect", 'c', POPT_ARG_STRING, &option_connect, 0, N_("Connect to server"), N_("SERVER")},
    {"nickname", 'n', POPT_ARG_STRING, &option_nick, 0, N_("Set nickname to use"), N_("NICKNAME")},
    {"team", 't', POPT_ARG_STRING, &option_team, 0, N_("Set team name"), N_("TEAM")},
    {"spectate", 's', POPT_ARG_NONE, &option_spec, 0, N_("Connect as spectator"), NULL},
    {"password", 'p', POPT_ARG_STRING, &option_pass, 0, N_("Spectator password"), N_("PASSWORD")},
    {NULL, 0, 0, NULL, 0, NULL, NULL}
};

int main (int argc, char *argv[])
{
    GtkWidget *page, *label, *box;

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    srand (time(NULL));

    gnome_init_with_popt_table (APPID, APPVERSION, argc, argv, options, 0 , NULL);
    gdk_imlib_init ();

    /* load settings */
    config_loadconfig ();

    /* initialise some stuff */
    client_initpipes ();
    fields_init ();

    /* first set up the display */

    /* create the main window */
    app = gnome_app_new (APPID, APPNAME);

    gtk_signal_connect (GTK_OBJECT(app), "destroy",
                        GTK_SIGNAL_FUNC(destroymain), NULL);
    gtk_signal_connect (GTK_OBJECT(app), "key_press_event",
                        GTK_SIGNAL_FUNC(keypress), NULL);
    gtk_signal_connect (GTK_OBJECT(app), "key_release_event",
                        GTK_SIGNAL_FUNC(keyrelease), NULL);
    gtk_widget_set_events (app, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

    gtk_window_set_policy (GTK_WINDOW(app), FALSE, FALSE, TRUE);

    /* create the notebook */
    notebook = gtk_notebook_new ();
    gtk_notebook_set_tab_pos (GTK_NOTEBOOK(notebook), GTK_POS_TOP);

    /* put it in the main window */
    gnome_app_set_contents (GNOME_APP(app), notebook);

    /* make menus + toolbar */
    make_menus (GNOME_APP(app));

    /* create the pages in the notebook */
    fieldswidget = fields_page_new ();
    gtk_widget_set_sensitive (fieldswidget, TRUE);
    gtk_widget_show (fieldswidget);
    pfields = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER(pfields), 0);
    gtk_container_add (GTK_CONTAINER(pfields), fieldswidget);
    gtk_widget_show (pfields);
    gtk_object_set_data( GTK_OBJECT(fieldswidget), "title", "Playing Fields"); // FIXME
    label = pixmapdata_label (fields_xpm, _("Playing Fields"));
    gtk_widget_show (label);
    gtk_notebook_append_page (GTK_NOTEBOOK(notebook), pfields, label);

    partywidget = partyline_page_new ();
    gtk_widget_show (partywidget);
    pparty = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER(pparty), 0);
    gtk_container_add (GTK_CONTAINER(pparty), partywidget);
    gtk_widget_show (pparty);
    gtk_object_set_data( GTK_OBJECT(partywidget), "title", "Partyline"); // FIXME
    label = pixmapdata_label (partyline_xpm, _("Partyline"));
    gtk_widget_show (label);
    gtk_notebook_append_page (GTK_NOTEBOOK(notebook), pparty, label);

    winlistwidget = winlist_page_new ();
    gtk_widget_show (winlistwidget);
    pwinlist = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER(pwinlist), 0);
    gtk_container_add (GTK_CONTAINER(pwinlist), winlistwidget);
    gtk_widget_show (pwinlist);
    gtk_object_set_data( GTK_OBJECT(winlistwidget), "title", "Winlist"); // FIXME
    label = pixmapdata_label (winlist_xpm, _("Winlist"));
    gtk_widget_show (label);
    gtk_notebook_append_page (GTK_NOTEBOOK(notebook), pwinlist, label);

    /* add signal to focus the text entry when switching to the partyline page*/
    gtk_signal_connect (GTK_OBJECT (notebook), "switch_page",
		    GTK_SIGNAL_FUNC (partyline_switch_entryfocus),
		    NULL);

    gtk_widget_show (notebook);
    gtk_widget_show (app);

    gtk_widget_set_usize(partywidget, 480, 360);
    gtk_widget_set_usize(winlistwidget, 480, 360);

    /* initialise some stuff */
    textbox_setup ();
    commands_checkstate ();

    /* check command line params */
#ifdef DEBUG
    printf ("option_connect: %s\n"
            "option_nick: %s\n"
            "option_team: %s\n"
            "option_pass: %s\n"
            "option_spec: %i\n",
            option_connect, option_nick, option_team,
            option_pass, option_spec);
#endif
    if (option_nick) strcpy (nick, option_nick);
    if (option_team) strcpy (team, option_team);
    if (option_pass) strcpy (specpassword, option_pass);
    if (option_spec) spectating = TRUE;
    if (option_connect) {
        client_init (option_connect, nick);
    }

    /* gtk_main() */
    gtk_main ();

    /* cleanup */
    client_destroypipes ();
    fields_cleanup ();
    client_connectcancel (); /* kills the client process */
    sound_stopmidi ();

    /* save settings */
    config_saveconfig ();

    return 0;
}

GtkWidget *pixmapdata_label (char **d, char *str)
{
    GdkPixmap *pm;
    GdkBitmap *mask;

    gdk_imlib_data_to_pixmap (d, &pm, &mask);

    return pixmap_label (pm, mask, str);
}

/* called when the main window is destroyed */
void destroymain (GtkWidget *widget, gpointer data)
{
    gtk_main_quit ();
}

/*
 The key press/release handlers requires a little hack:
 There is no indication whether each keypress/release is a real press
 or a real release, or whether it is just typematic action.
 However, if it is a result of typematic action, the keyrelease and the
 following keypress event have the same value in the time field of the
 GdkEventKey struct.
 The solution is: when a keyrelease event is received, the event is stored
 and a timeout handler is installed.  if a subsequent keypress event is
 received with the same value in the time field, the keyrelease event is
 discarded.  The keyrelease event is sent if the timeout is reached without
 being cancelled.
 This results in slightly slower responses for key releases, but it should not
 be a big problem.
 */

GdkEventKey k;
gint keytimeoutid = 0;

gint keytimeout (gpointer data)
{
    tetrinet_upkey (k.keyval);
    keytimeoutid = 0;
    return FALSE;
}

gint keypress (GtkWidget *widget, GdkEventKey *key)
{
    int game_area;

    if (widget == app)
    {
        /* Main window - check the notebook */
        game_area = (GTK_NOTEBOOK(notebook)->cur_page->child == pfields);
    }
    else
    {
        /* Sub-window - find out which */
        GtkArg arg;

        arg.name = "title";
        gtk_object_getv( GTK_OBJECT(widget), 1, &arg );
        game_area =  !strcmp( GTK_VALUE_STRING(arg), "Playing Fields" ); // FIXME
        g_free( GTK_VALUE_STRING(arg) );
    }

    if (game_area)
    {
        /* keys for the playing field - key releases needed - install timeout */
        if (keytimeoutid && key->time == k.time)
            gtk_timeout_remove (keytimeoutid);
        if (tetrinet_key (key->keyval, key->string)) goto keyprocessed;
    }
    if (gtetrinet_key(key->keyval, key->state & (GDK_MOD1_MASK |
                                                 GDK_CONTROL_MASK |
                                                 GDK_SHIFT_MASK)))
        goto keyprocessed;
    return FALSE;
keyprocessed:
    gtk_signal_emit_stop_by_name (GTK_OBJECT(widget), "key_press_event");
    return TRUE;
}

gint keyrelease (GtkWidget *widget, GdkEventKey *key)
{
    int game_area;

    if (widget == app)
    {
        /* Main window - check the notebook */
        game_area = (GTK_NOTEBOOK(notebook)->cur_page->child == pfields);
    }
    else
    {
        /* Sub-window - find out which */
        GtkArg arg;

        arg.name = "title";
        gtk_object_getv( GTK_OBJECT(widget), 1, &arg );
        game_area =  !strcmp( GTK_VALUE_STRING(arg), "Playing Fields" );
        g_free( GTK_VALUE_STRING(arg) );
    }

    if (game_area)
    {
        k = *key;
        keytimeoutid = gtk_timeout_add (10, keytimeout, 0);
        gtk_signal_emit_stop_by_name (GTK_OBJECT(widget), "key_release_event");
        return TRUE;
    }
    return FALSE;
}

/*
 TODO: make this switch between detached pages too
 */
static int gtetrinet_key (int keyval, int mod)
{
    if (mod != GDK_MOD1_MASK)
      return (FALSE);
    
    switch (keyval)
    {
    case GDK_1: gtk_notebook_set_page (GTK_NOTEBOOK(notebook), 0); break;
    case GDK_2:
        gtk_notebook_set_page (GTK_NOTEBOOK(notebook), 1);
        partyline_entryfocus();
        break;
    case GDK_3: gtk_notebook_set_page (GTK_NOTEBOOK(notebook), 2); break;
    default:
        return FALSE;
    }
    return TRUE;
}

/* funky page detach stuff */

/* Type to hold primary widget and its label in the notebook page */
typedef struct {
    GtkWidget *parent;
    GtkWidget *widget;
    int pageNo;
} WidgetPageData;

void destroy_page_window (GtkWidget *window, gpointer data)
{
    WidgetPageData *pageData = (WidgetPageData *)data;

    /* Put widget back into a page */
    gtk_widget_reparent (pageData->widget, pageData->parent);

    /* Select it */
    gtk_notebook_set_page (GTK_NOTEBOOK(notebook), pageData->pageNo);

    /* Free return data */
    g_free (data);
}

void move_current_page_to_window (void)
{
    WidgetPageData *pageData;
    GtkWidget *page, *child, *newWindow;
    GList *dlist;
    gint pageNo;
    char *title;

    /* Extract current page's widget & it's parent from the notebook */
    pageNo = gtk_notebook_get_current_page (GTK_NOTEBOOK(notebook));
    page   = gtk_notebook_get_nth_page (GTK_NOTEBOOK(notebook), pageNo );
    dlist  = gtk_container_children (GTK_CONTAINER(page));
    if (!dlist ||  !(dlist->data))
    {
        /* Must already be a window */
        if (dlist)
           g_list_free (dlist);
        return;
    }
    child = (GtkWidget *)dlist->data;
    g_list_free (dlist);

    /* Create new window for widget, plus container, etc. */
    newWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    title = gtk_object_get_data(GTK_OBJECT(child), "title");
    if (!title)
        title = "GTetrinet";
    gtk_window_set_title (GTK_WINDOW (newWindow), title);
    gtk_container_set_border_width (GTK_CONTAINER (newWindow), 0);

    /* Attach key events to window */
    gtk_signal_connect (GTK_OBJECT(newWindow), "key_press_event",
                        GTK_SIGNAL_FUNC(keypress), NULL);
    gtk_signal_connect (GTK_OBJECT(newWindow), "key_release_event",
                        GTK_SIGNAL_FUNC(keyrelease), NULL);
    gtk_widget_set_events (newWindow, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
    gtk_window_set_policy (GTK_WINDOW(newWindow), FALSE, TRUE, FALSE);

    /* Create store to point us back to page for later */
    pageData = g_new( WidgetPageData, 1 );
    pageData->parent = page;
    pageData->widget = child;
    pageData->pageNo = pageNo;

    /* Move main widget to window */
    gtk_widget_reparent (child, newWindow);

    /* Pass ID of parent (to put widget back) to window's destroy */
    gtk_signal_connect (GTK_OBJECT(newWindow), "destroy",
                        GTK_SIGNAL_FUNC(destroy_page_window),
                        (gpointer)(pageData));

    gtk_widget_show_all( newWindow );

    /* cure annoying side effect */
    if (gmsgstate)
        fields_gmsginput(TRUE);
    else
        fields_gmsginput(FALSE);

}
