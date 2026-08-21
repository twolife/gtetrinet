
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <time.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <gobject/gtype.h>
#include <signal.h>

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
gboolean keypress (GtkEventControllerKey *controller,
                           guint keyval,
                           guint keycode,
                           GdkModifierType state,
                           gpointer user_data);
void keyrelease (GtkEventControllerKey *controller,
                        guint keyval,
                        guint keycode,
                        GdkModifierType state,
                        gpointer user_data);
void switch_focus (GtkNotebook *notebook,
                   void *page,
                   guint page_num);

static GtkWidget *pfields, *pparty, *pwinlist;
static GtkWidget *winlistwidget, *partywidget, *fieldswidget;
static GtkWidget *notebook;

GtkWidget *app;
static GtkApplication *gtk_app;
static GtkEventController *main_key_controller;

char *option_connect = 0, *option_nick = 0, *option_team = 0, *option_pass = 0;
int option_spec = 0;

static const GOptionEntry options[] = {
    {
        "connect", 'c', 0, G_OPTION_ARG_STRING,
        &option_connect, N_("Connect to server"), N_("SERVER")
    },
    {
        "nickname", 'n', 0, G_OPTION_ARG_STRING,
        &option_nick, N_("Set nickname to use"), N_("NICKNAME")
    },
    {
        "team", 't', 0, G_OPTION_ARG_STRING,
        &option_team, N_("Set team name"), N_("TEAM")
    },
    {
        "spectate", 's', 0, G_OPTION_ARG_NONE,
        &option_spec,  N_("Connect as a spectator"), NULL
    },
    {
        "password", 'p', 0, G_OPTION_ARG_STRING,
        &option_pass,  N_("Spectator password"), N_("PASSWORD")
    },
    { NULL }
};

int gamemode = ORIGINAL;

int fields_width, fields_height;

gulong keypress_signal;

GSettings* settings;
GSettings* settings_keys;
GSettings* settings_themes;

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

/*
 * based on https://developer.gnome.org/gio/stable/gio-GSettingsSchema-GSettingsSchemaSource.html
 * I have no idea why this is not the default behavior
 */
GSettings *get_schema_settings(const gchar *schema_id)
{
    GSettingsSchema *schema = NULL;
    GSettingsSchemaSource *schema_source;

    schema_source = g_settings_schema_source_new_from_directory (
                        GSETTINGSSCHEMADIR,
                        g_settings_schema_source_get_default (),
                        FALSE,
                        NULL
                    );

    if (schema_source != NULL) {
        schema = g_settings_schema_source_lookup (
                     schema_source,
                     schema_id,
                     FALSE
                 );
        g_settings_schema_source_unref (schema_source);
    }
    if (schema == NULL)
        return g_settings_new (schema_id);

    {
        GSettings *result =
            g_settings_new_full (schema, NULL, NULL);

        g_settings_schema_unref (schema);
        return result;
    }
}

void destroymain (void)
{
    client_disconnect();
    if (gtk_app)
        g_application_quit (G_APPLICATION(gtk_app));
}

static gboolean
main_close_request (GtkWindow *window, gpointer data)
{
    (void)window;
    (void)data;

    destroymain();
    return TRUE;
}

static void
activate (GtkApplication *application, gpointer user_data)
{
    GtkWidget *label;
    GtkEventController *key_controller;

    (void)user_data;

    textbox_setup ();

    settings = get_schema_settings (GSETTINGS_DOMAIN);
    settings_keys = get_schema_settings (GSETTINGS_DOMAIN_KEYS);
    settings_themes = get_schema_settings (GSETTINGS_DOMAIN_THEMES);

    g_signal_connect_swapped (settings, "changed", G_CALLBACK(config_loadconfig), NULL);
    g_signal_connect_swapped (settings_keys, "changed", G_CALLBACK(config_loadconfig_keys), NULL);
    g_signal_connect_swapped (settings_themes, "changed", G_CALLBACK(config_loadconfig_themes), NULL);

    config_loadconfig ();
    config_loadconfig_keys ();

    app = gtk_application_window_new (application);
    gtk_window_set_title (GTK_WINDOW (app), APPNAME);
    gtk_window_set_resizable (GTK_WINDOW (app), TRUE);
    g_signal_connect (app, "close-request", G_CALLBACK (main_close_request), NULL);

    key_controller = gtk_event_controller_key_new ();
    main_key_controller = key_controller;
    keypress_signal = g_signal_connect (key_controller, "key-pressed",
                                        G_CALLBACK(keypress), app);
    g_signal_connect (key_controller, "key-released",
                      G_CALLBACK(keyrelease), app);
    gtk_widget_add_controller (app, key_controller);

    notebook = gtk_notebook_new ();
    gtk_notebook_set_tab_pos (GTK_NOTEBOOK(notebook), GTK_POS_TOP);
    gtk_window_set_child (GTK_WINDOW(app), notebook);

    make_menus (GTK_WINDOW(app));

    fieldswidget = fields_page_new ();
    gtk_widget_set_sensitive (fieldswidget, TRUE);
    pfields = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append (GTK_BOX(pfields), fieldswidget);
    g_object_set_data (G_OBJECT(fieldswidget), "title", "Playing Fields");
    label = pixmapdata_label (fields_xpm, "Playing Fields");
    gtk_notebook_append_page (GTK_NOTEBOOK(notebook), pfields, label);

    partywidget = partyline_page_new ();
    pparty = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append (GTK_BOX(pparty), partywidget);
    g_object_set_data (G_OBJECT(partywidget), "title", "Partyline");
    label = pixmapdata_label (partyline_xpm, "Partyline");
    gtk_notebook_append_page (GTK_NOTEBOOK(notebook), pparty, label);

    winlistwidget = winlist_page_new ();
    pwinlist = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append (GTK_BOX(pwinlist), winlistwidget);
    g_object_set_data (G_OBJECT(winlistwidget), "title", "Winlist");
    label = pixmapdata_label (winlist_xpm, "Winlist");
    gtk_notebook_append_page (GTK_NOTEBOOK(notebook), pwinlist, label);

    g_signal_connect_after (notebook, "switch-page",
                            G_CALLBACK(switch_focus), NULL);
    gtk_widget_set_focusable (notebook, FALSE);

    partyline_show_channel_list (list_enabled);
    config_loadconfig_themes ();
    commands_checkstate ();

    if (option_nick) GTET_O_STRCPY(nick, option_nick);
    if (option_team) GTET_O_STRCPY(team, option_team);
    if (option_pass) GTET_O_STRCPY(specpassword, option_pass);
    if (option_spec) spectating = TRUE;
    if (option_connect)
        client_init (option_connect, nick);

    gtk_window_present (GTK_WINDOW(app));
}

int main (int argc, char *argv[])
{
    int status;

    bindtextdomain(PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE, "UTF-8");
    textdomain(PACKAGE);
    srand (time(NULL));

    gtk_app = gtk_application_new ("net.sourceforge.gtetrinet.GTetrinet", G_APPLICATION_DEFAULT_FLAGS);
    g_application_add_main_option_entries (G_APPLICATION (gtk_app), options);
    g_signal_connect (gtk_app, "activate", G_CALLBACK(activate), NULL);

    g_main_context_set_poll_func (NULL, gtetrinet_poll_func);
    status = g_application_run (G_APPLICATION(gtk_app), argc, argv);

    client_disconnect ();
    fields_cleanup ();
    g_clear_object (&settings);
    g_clear_object (&settings_keys);
    g_clear_object (&settings_themes);
    g_clear_object (&gtk_app);

    return status;
}

GtkWidget *pixmapdata_label (char **d, char *str)
{
    GdkPixbuf *pb;
    GtkWidget *box, *widget;

    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_hexpand (box, FALSE);

    pb = gdk_pixbuf_new_from_xpm_data ((const char **)d);
    widget = gtk_image_new_from_pixbuf (pb);
    gtk_box_append (GTK_BOX(box), widget);
    g_object_unref (pb);
  
    widget = gtk_label_new (str);
    gtk_box_append (GTK_BOX(box), widget);

    return box;
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

static guint pending_keyval;
static gint keytimeoutid = 0;

static gint keytimeout (gpointer data)
{
    (void)data;
    tetrinet_upkey (pending_keyval);
    keytimeoutid = 0;
    return G_SOURCE_REMOVE;
}

gboolean keypress (GtkEventControllerKey *controller,
                   guint keyval, guint keycode,
                   GdkModifierType state, gpointer user_data)
{
    GtkWidget *widget = GTK_WIDGET (user_data);
    int game_area;
    (void)controller;
    (void)keycode;

    if (widget == app) {
        int cur_page = gtk_notebook_get_current_page (GTK_NOTEBOOK(notebook));
        int pfields_page = gtk_notebook_page_num (GTK_NOTEBOOK(notebook), pfields);
        game_area = (cur_page == pfields_page);
    } else {
        const char *title = g_object_get_data (G_OBJECT(widget), "title");
        game_area = title && !strcmp (title, "Playing Fields");
    }

    if (game_area && keytimeoutid) {
        g_source_remove (keytimeoutid);
        keytimeoutid = 0;
    }

    if (gtetrinet_key (keyval, state & GDK_ALT_MASK))
        return TRUE;

    if (game_area && ingame &&
        (gdk_keyval_to_lower (keyval) == keys[K_GAMEMSG])) {
        if (main_key_controller)
            g_signal_handler_block (main_key_controller, keypress_signal);
        fields_gmsginputactivate (TRUE);
        return TRUE;
    }

    if (game_area && tetrinet_key (keyval))
        return TRUE;

    return FALSE;
}

void keyrelease (GtkEventControllerKey *controller,
                 guint keyval, guint keycode,
                 GdkModifierType state, gpointer user_data)
{
    GtkWidget *widget = GTK_WIDGET (user_data);
    int game_area;
    (void)controller;
    (void)keycode;
    (void)state;

    if (widget == app) {
        int cur_page = gtk_notebook_get_current_page (GTK_NOTEBOOK(notebook));
        int pfields_page = gtk_notebook_page_num (GTK_NOTEBOOK(notebook), pfields);
        game_area = (cur_page == pfields_page);
    } else {
        const char *title = g_object_get_data (G_OBJECT(widget), "title");
        game_area = title && !strcmp (title, "Playing Fields");
    }

    if (game_area) {
        if (keytimeoutid)
            g_source_remove (keytimeoutid);
        pending_keyval = keyval;
        keytimeoutid = g_timeout_add (10, keytimeout, NULL);
    }
}

/*
 * TODO: make this switch between detached pages too
 */
static int gtetrinet_key (int keyval, int mod)
{
  if (mod != GDK_ALT_MASK)
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

    g_object_ref (pageData->widget);
    gtk_window_set_child (GTK_WINDOW(window), NULL);
    gtk_box_append (GTK_BOX(pageData->parent), pageData->widget);
    g_object_unref (pageData->widget);

    gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook), pageData->pageNo);
    g_free (pageData);
}

void move_current_page_to_window (void)
{
    WidgetPageData *pageData;
    GtkWidget *page, *child, *newWindow;
    GtkEventController *key_controller;
    gint pageNo;
    const char *title;

    pageNo = gtk_notebook_get_current_page (GTK_NOTEBOOK(notebook));
    page = gtk_notebook_get_nth_page (GTK_NOTEBOOK(notebook), pageNo);
    if (!page)
        return;

    child = gtk_widget_get_first_child (page);
    if (!child)
        return;

    newWindow = gtk_application_window_new (gtk_app);
    title = g_object_get_data (G_OBJECT(child), "title");
    if (!title)
        title = "GTetrinet";
    gtk_window_set_title (GTK_WINDOW(newWindow), title);
    gtk_window_set_resizable (GTK_WINDOW(newWindow), TRUE);
    g_object_set_data_full (G_OBJECT(newWindow), "title", g_strdup(title), g_free);

    key_controller = gtk_event_controller_key_new ();
    g_signal_connect (key_controller, "key-pressed",
                      G_CALLBACK(keypress), newWindow);
    g_signal_connect (key_controller, "key-released",
                      G_CALLBACK(keyrelease), newWindow);
    gtk_widget_add_controller (newWindow, key_controller);

    pageData = g_new (WidgetPageData, 1);
    pageData->parent = page;
    pageData->widget = child;
    pageData->pageNo = pageNo;

    g_object_ref (child);
    gtk_widget_unparent (child);
    gtk_window_set_child (GTK_WINDOW(newWindow), child);
    g_object_unref (child);

    g_signal_connect (newWindow, "destroy",
                      G_CALLBACK(destroy_page_window), pageData);

    gtk_window_present (GTK_WINDOW(newWindow));
    fields_gmsginput (gmsgstate ? TRUE : FALSE);
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
    if (main_key_controller)
      g_signal_handler_unblock (main_key_controller, keypress_signal);
}

void switch_focus (GtkNotebook *notebook,
                   void *page,
                   guint page_num)
{
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
