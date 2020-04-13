
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
#include <stdlib.h>
#include <time.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <signal.h>
#include <gconf/gconf-client.h>
#include <popt.h>

#include "gtetrinet.h"
#include "gtet_config.h"
#include "client.h"
#include "tetrinet.h"
#include "tetris.h"
#include "fields.h"
#include "partyline.h"
#include "winlist.h"
#include "misc.h"
#include "commands.h"
#include "sound.h"
#include "string.h"

#include "images/fields.xpm"
#include "images/partyline.xpm"
#include "images/winlist.xpm"

static GtkWidget *pixmapdata_label (char **d, char *str);
static int gtetrinet_key (int keyval, int mod);
gint keypress (GtkWidget *widget, GdkEventKey *key);
gint keyrelease (GtkWidget *widget, GdkEventKey *key);
void switch_focus (GtkNotebook *notebook,
                   void *page,
                   guint page_num);

static GtkWidget *pfields, *pparty, *pwinlist;
static GtkWidget *winlistwidget, *partywidget, *fieldswidget;
static GtkWidget *notebook;

GtkWidget *app;

char *option_connect = 0, *option_nick = 0, *option_team = 0, *option_pass = 0;
int option_spec = 0;

int gamemode = ORIGINAL;

int fields_width, fields_height;

gulong keypress_signal;

GConfClient *gconf_client;

static const struct poptOption options[] = {
    {"connect", 'c', POPT_ARG_STRING, &option_connect, 0, ("Connect to server"), ("SERVER")},
    {"nickname", 'n', POPT_ARG_STRING, &option_nick, 0, ("Set nickname to use"), ("NICKNAME")},
    {"team", 't', POPT_ARG_STRING, &option_team, 0, ("Set team name"), ("TEAM")},
    {"spectate", 's', POPT_ARG_NONE, &option_spec, 0, ("Connect as a spectator"), NULL},
    {"password", 'p', POPT_ARG_STRING, &option_pass, 0, ("Spectator password"), ("PASSWORD")},
    {NULL, 0, 0, NULL, 0, NULL, NULL}
};

static int gtetrinet_poll_func(GPollFD *passed_fds,
                               guint nfds,
                               int timeout)
{ /* passing a timeout wastes time, even if data is ready... don't do that */
  int ret = 0;
  struct pollfd *fds = (struct pollfd *)passed_fds;

  ret = poll(fds, nfds, 0);
  if (!ret && timeout)
    ret = poll(fds, nfds, timeout);

  return (ret);
}

int main (int argc, char *argv[])
{
    GtkWidget *label;
    GdkPixbuf *icon_pixbuf;
    GError *err = NULL;
    
    bindtextdomain(PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE, "UTF-8");
    textdomain(PACKAGE);

    srand (time(NULL));

    /*
    gnome_program_init (APPID, APPVERSION, LIBGNOMEUI_MODULE,
                        argc, argv, GNOME_PARAM_POPT_TABLE, options,
                        GNOME_PARAM_NONE);
    */
    GOptionEntry options[] = { {NULL}};
    if (!gtk_init_with_args(&argc,&argv,"gtetrinet",options,NULL,&err))
    {
        fprintf (stderr, "Failed to init GTK: %s\n", err->message);
        g_error_free(err);
        err = NULL;
        return 1;
    }
    textbox_setup (); /* needs to be done before text boxes are created */
    
    /* Initialize the GConf library */
    if (!gconf_init (argc, argv, &err))
    {
      fprintf (stderr, "Failed to init GConf: %s\n", err->message);
      g_error_free (err); 
      err = NULL;
    }
  
    /* Start a GConf client */
    gconf_client = gconf_client_get_default ();
  
    /* Add the GTetrinet directories to the list of directories that GConf client must watch */
    gconf_client_add_dir (gconf_client, "/apps/gtetrinet/sound",
                          GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
    
    gconf_client_add_dir (gconf_client, "/apps/gtetrinet/themes",
                          GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  
    gconf_client_add_dir (gconf_client, "/apps/gtetrinet/keys",
                          GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

    gconf_client_add_dir (gconf_client, "/apps/gtetrinet/partyline",
                          GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

    /* Request notification of change for these gconf keys */
    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/sound/midi_player",
                             (GConfClientNotifyFunc) sound_midi_player_changed,
			     NULL, NULL, NULL);
                             
    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/sound/enable_sound",
                             (GConfClientNotifyFunc) sound_enable_sound_changed,
			     NULL, NULL, NULL);
                             
    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/sound/enable_midi",
                             (GConfClientNotifyFunc) sound_enable_midi_changed,
			     NULL, NULL, NULL);
                             
    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/themes/theme_dir",
                             (GConfClientNotifyFunc) themes_theme_dir_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/down",
                             (GConfClientNotifyFunc) keys_down_changed, NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/left",
                             (GConfClientNotifyFunc) keys_left_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/right",
                             (GConfClientNotifyFunc) keys_right_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/rotate_left",
                             (GConfClientNotifyFunc) keys_rotate_left_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/rotate_right",
                             (GConfClientNotifyFunc) keys_rotate_right_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/drop",
                             (GConfClientNotifyFunc) keys_drop_changed, NULL,
			     NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/message",
			     (GConfClientNotifyFunc) keys_message_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/discard",
			     (GConfClientNotifyFunc) keys_discard_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/special1",
                             (GConfClientNotifyFunc) keys_special1_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/special2",
                             (GConfClientNotifyFunc) keys_special2_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/special3",
                             (GConfClientNotifyFunc) keys_special3_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/special4",
                             (GConfClientNotifyFunc) keys_special4_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/special5",
                             (GConfClientNotifyFunc) keys_special5_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/special6",
                             (GConfClientNotifyFunc) keys_special6_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/partyline/enable_timestamps",
                             (GConfClientNotifyFunc) partyline_enable_timestamps_changed,
			     NULL, NULL, NULL);
    
    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/partyline/enable_channel_list",
                             (GConfClientNotifyFunc) partyline_enable_channel_list_changed,
			     NULL, NULL, NULL);

    /* load settings */
    config_loadconfig ();

    /* initialise some stuff */
    fields_init ();

    /* first set up the display */

    /* create the main window */
    app = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (app), APPNAME);

    g_signal_connect (G_OBJECT(app), "destroy",
                        G_CALLBACK(destroymain), NULL);
    keypress_signal = g_signal_connect (G_OBJECT(app), "key-press-event",
                                        G_CALLBACK(keypress), NULL);
    g_signal_connect (G_OBJECT(app), "key-release-event",
                        G_CALLBACK(keyrelease), NULL);
    gtk_widget_set_events (app, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

    gtk_window_set_resizable (GTK_WINDOW (app), TRUE);
    
    /* create and set the window icon */
    icon_pixbuf = gdk_pixbuf_new_from_file (PIXMAPSDIR "/gtetrinet.png", NULL);
    if (icon_pixbuf)
    {
      gtk_window_set_icon (GTK_WINDOW (app), icon_pixbuf);
      g_object_unref (icon_pixbuf);
    }

    /* create the notebook */
    notebook = gtk_notebook_new ();
    gtk_notebook_set_tab_pos (GTK_NOTEBOOK(notebook), GTK_POS_TOP);

    /* put it in the main window */
    gtk_container_add (GTK_CONTAINER(app), notebook);

    /* make menus + toolbar */
    make_menus (GTK_WINDOW(app));

    /* create the pages in the notebook */
    fieldswidget = fields_page_new ();
    gtk_widget_set_sensitive (fieldswidget, TRUE);
    gtk_widget_show (fieldswidget);
    pfields = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER(pfields), 0);
    gtk_container_add (GTK_CONTAINER(pfields), fieldswidget);
    gtk_widget_show (pfields);
    g_object_set_data (G_OBJECT(fieldswidget), "title", "Playing Fields"); // FIXME
    label = pixmapdata_label (fields_xpm, "Playing Fields");
    gtk_widget_show (label);
    gtk_notebook_append_page (GTK_NOTEBOOK(notebook), pfields, label);

    partywidget = partyline_page_new ();
    gtk_widget_show (partywidget);
    pparty = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER(pparty), 0);
    gtk_container_add (GTK_CONTAINER(pparty), partywidget);
    gtk_widget_show (pparty);
    g_object_set_data (G_OBJECT(partywidget), "title", "Partyline"); // FIXME
    label = pixmapdata_label (partyline_xpm, "Partyline");
    gtk_widget_show (label);
    gtk_notebook_append_page (GTK_NOTEBOOK(notebook), pparty, label);

    winlistwidget = winlist_page_new ();
    gtk_widget_show (winlistwidget);
    pwinlist = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER(pwinlist), 0);
    gtk_container_add (GTK_CONTAINER(pwinlist), winlistwidget);
    gtk_widget_show (pwinlist);
    g_object_set_data (G_OBJECT(winlistwidget), "title", "Winlist"); // FIXME
    label = pixmapdata_label (winlist_xpm, "Winlist");
    gtk_widget_show (label);
    gtk_notebook_append_page (GTK_NOTEBOOK(notebook), pwinlist, label);

    /* add signal to focus the text entry when switching to the partyline page*/
    g_signal_connect_after(G_OBJECT (notebook), "switch-page",
		           G_CALLBACK (switch_focus),
		           NULL);

    gtk_widget_show (notebook);
    g_object_set (G_OBJECT (notebook), "can-focus", FALSE, NULL);

    partyline_show_channel_list (list_enabled);
    gtk_widget_show (app);

//    gtk_widget_set_size_request (partywidget, 480, 360);
//    gtk_widget_set_size_request (winlistwidget, 480, 360);

    /* initialise some stuff */
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
    if (option_nick) GTET_O_STRCPY(nick, option_nick);
    if (option_team) GTET_O_STRCPY(team, option_team);
    if (option_pass) GTET_O_STRCPY(specpassword, option_pass);
    if (option_spec) spectating = TRUE;
    if (option_connect) {
        client_init (option_connect, nick);
    }

    /* Don't schedule if data is ready, glib should do this itself,
     * but welcome to anything that works... */
    g_main_context_set_poll_func(NULL, gtetrinet_poll_func);

    /* gtk_main() */
    gtk_main ();

    client_disconnect ();
    /* cleanup */
    fields_cleanup ();
    sound_stopmidi ();

    return 0;
}

GtkWidget *pixmapdata_label (char **d, char *str)
{
    GdkPixbuf *pb;
    GtkWidget *box, *widget;

    box = gtk_hbox_new (FALSE, 0);

    pb = gdk_pixbuf_new_from_xpm_data ((const char **)d);
    widget = gtk_image_new_from_pixbuf (pb);
    gtk_widget_show (widget);
    gtk_box_pack_start (GTK_BOX(box), widget, TRUE, TRUE, 0);
  
    widget = gtk_label_new (str);
    gtk_widget_show (widget);
    gtk_box_pack_start (GTK_BOX(box), widget, TRUE, TRUE, 0);

    return box;
}

/* called when the main window is destroyed */
void destroymain (void)
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
    data = data; /* to get rid of the warning */
    tetrinet_upkey (k.keyval);
    keytimeoutid = 0;
    return FALSE;
}

gint keypress (GtkWidget *widget, GdkEventKey *key)
{
    int game_area;

    if (widget == app)
    {
      int cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
      int pfields_page = gtk_notebook_page_num(GTK_NOTEBOOK(notebook),
                                               pfields);
      /* Main window - check the notebook */
      game_area = (cur_page == pfields_page);
    }
    else
    {
        /* Sub-window - find out which */
        char *title = NULL;

        title = g_object_get_data(G_OBJECT(widget), "title");
        game_area =  title && !strcmp( title, "Playing Fields");
    }

    if (game_area)
    { /* keys for the playing field - key releases needed - install timeout */
      if (keytimeoutid && key->time == k.time)
        g_source_remove (keytimeoutid);
    }

    /* Check if it's a GTetrinet key */
    if (gtetrinet_key (key->keyval, key->state & (GDK_MOD1_MASK)))
    {
      g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
      return TRUE;
    }

/*    if ((key->state & (GDK_MOD1_MASK | GDK_CONTROL_MASK)) > 0)
    return FALSE;*/
    
    if (game_area && ingame && (gdk_keyval_to_lower (key->keyval) == keys[K_GAMEMSG]))
    {
      g_signal_handler_block (app, keypress_signal);
      fields_gmsginputactivate (TRUE);
      g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
    }

    if (game_area && tetrinet_key (key->keyval))
    {
      g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
      return TRUE;
    }
    
    return FALSE;
}

gint keyrelease (GtkWidget *widget, GdkEventKey *key)
{
    int game_area;

    if (widget == app)
    {
      int cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
      int pfields_page = gtk_notebook_page_num(GTK_NOTEBOOK(notebook),
                                               pfields);
      /* Main window - check the notebook */
      game_area = (cur_page == pfields_page);
    }
    else
    {
        /* Sub-window - find out which */
        char *title = NULL;

        title = g_object_get_data(G_OBJECT(widget), "title");
        game_area =  title && !strcmp( title, "Playing Fields");
    }

    if (game_area)
    {
        k = *key;
        keytimeoutid = g_timeout_add (10, keytimeout, 0);
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-release-event");
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
    return FALSE;
    
  switch (keyval)
  {
  case GDK_KEY_1: gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook), 0); break;
  case GDK_KEY_2: gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook), 1); break;
  case GDK_KEY_3: gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook), 2); break;
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
    window = window;

    /* Put widget back into a page */
    gtk_widget_reparent (pageData->widget, pageData->parent);

    /* Select it */
    gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook), pageData->pageNo);

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
    dlist  = gtk_container_get_children (GTK_CONTAINER(page));
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
    title = g_object_get_data (G_OBJECT(child), "title");
    if (!title)
        title = "GTetrinet";
    gtk_window_set_title (GTK_WINDOW (newWindow), title);
    gtk_container_set_border_width (GTK_CONTAINER (newWindow), 0);

    /* Attach key events to window */
    g_signal_connect (G_OBJECT(newWindow), "key-press-event",
                        G_CALLBACK(keypress), NULL);
    g_signal_connect (G_OBJECT(newWindow), "key-release-event",
                        G_CALLBACK(keyrelease), NULL);
    gtk_widget_set_events (newWindow, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
    gtk_window_set_resizable (GTK_WINDOW(newWindow), TRUE);

    /* Create store to point us back to page for later */
    pageData = g_new( WidgetPageData, 1 );
    pageData->parent = page;
    pageData->widget = child;
    pageData->pageNo = pageNo;

    /* Move main widget to window */
    gtk_widget_reparent (child, newWindow);

    /* Pass ID of parent (to put widget back) to window's destroy */
    g_signal_connect (G_OBJECT(newWindow), "destroy",
                        G_CALLBACK(destroy_page_window),
                        (gpointer)(pageData));

    gtk_widget_show_all( newWindow );

    /* cure annoying side effect */
    if (gmsgstate)
        fields_gmsginput(TRUE);
    else
        fields_gmsginput(FALSE);

}

/* show the fields notebook tab */
void show_fields_page (void)
{
    gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook), 0);
}

/* show the partyline notebook tab */
void show_partyline_page (void)
{
    gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook), 1);
}

void unblock_keyboard_signal (void)
{
    g_signal_handler_unblock (app, keypress_signal);
}

void switch_focus (GtkNotebook *notebook,
                   void *page,
                   guint page_num)
{

    notebook = notebook;	/* Suppress compile warnings */
    page = page;		/* Suppress compile warnings */

    if (connected)
      switch (page_num)
      {
        case 0:
          if (gmsgstate) fields_gmsginputactivate (1);
          else partyline_entryfocus ();
          break;
        case 1: partyline_entryfocus (); break;
        case 2: winlist_focus (); break;
      }
}
