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
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>

#ifdef ENABLE_SERVERLIST
#include <libsoup/soup.h>
#endif

#include "gtetrinet.h"
#include "gtet_config.h"
#include "client.h"
#include "tetrinet.h"
#include "tetris.h"
#include "fields.h"
#include "misc.h"
#include "sound.h"
#include "partyline.h"

/* Adopted and renamed from libgnomeui */
#define GTET_PAD 8
#define GTET_PAD_SMALL 4


/* GTK4 compatibility helpers for the old GtkTable-based layouts. */
#define GTET_ATTACH_EXPAND (1u << 0)
#define GTET_ATTACH_SHRINK (1u << 1)
#define GTET_ATTACH_FILL   (1u << 2)
#define GTK_EXPAND GTET_ATTACH_EXPAND
#define GTK_SHRINK GTET_ATTACH_SHRINK
#define GTK_FILL   GTET_ATTACH_FILL
#define GTK_TABLE(obj) GTK_GRID (obj)

static GtkWidget *
gtet_table_new (guint rows G_GNUC_UNUSED,
                guint columns G_GNUC_UNUSED,
                gboolean homogeneous)
{
    GtkWidget *grid = gtk_grid_new ();
    gtk_grid_set_row_homogeneous (GTK_GRID (grid), homogeneous);
    gtk_grid_set_column_homogeneous (GTK_GRID (grid), homogeneous);
    return grid;
}

static void
gtet_table_attach (GtkGrid *grid,
                   GtkWidget *child,
                   guint left,
                   guint right,
                   guint top,
                   guint bottom,
                   guint xoptions,
                   guint yoptions,
                   guint xpadding,
                   guint ypadding)
{
    gtk_grid_attach (grid, child, left, top, right - left, bottom - top);
    gtk_widget_set_hexpand (child, (xoptions & GTK_EXPAND) != 0);
    gtk_widget_set_vexpand (child, (yoptions & GTK_EXPAND) != 0);
    if (xoptions & GTK_FILL)
        gtk_widget_set_halign (child, GTK_ALIGN_FILL);
    if (yoptions & GTK_FILL)
        gtk_widget_set_valign (child, GTK_ALIGN_FILL);
    if (xpadding) {
        gtk_widget_set_margin_start (child, xpadding);
        gtk_widget_set_margin_end (child, xpadding);
    }
    if (ypadding) {
        gtk_widget_set_margin_top (child, ypadding);
        gtk_widget_set_margin_bottom (child, ypadding);
    }
}

#define gtk_table_new(rows, columns, homogeneous) \
    gtet_table_new ((rows), (columns), (homogeneous))
#define gtk_table_set_row_spacings(table, spacing) \
    gtk_grid_set_row_spacing (GTK_GRID (table), (spacing))
#define gtk_table_set_col_spacings(table, spacing) \
    gtk_grid_set_column_spacing (GTK_GRID (table), (spacing))
#define gtk_table_attach(table, child, left, right, top, bottom, xopt, yopt, xpad, ypad) \
    gtet_table_attach (GTK_GRID (table), (child), (left), (right), (top), (bottom), \
                       (xopt), (yopt), (xpad), (ypad))

static void
gtet_show_error (GtkWindow *parent, const char *message)
{
    GtkAlertDialog *alert = gtk_alert_dialog_new ("%s", message);
    gtk_alert_dialog_set_modal (alert, TRUE);
    gtk_alert_dialog_show (alert, parent);
    g_object_unref (alert);
}

static GtkWidget *
gtet_window_new (const char *title,
                 GtkWindow *parent,
                 gboolean modal,
                 GtkWidget **content_box,
                 GtkWidget **action_box)
{
    GtkWidget *window, *root;

    window = gtk_window_new ();
    gtk_window_set_title (GTK_WINDOW (window), title);
    if (parent != NULL)
        gtk_window_set_transient_for (GTK_WINDOW (window), parent);
    gtk_window_set_modal (GTK_WINDOW (window), modal);

    root = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    *content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    *action_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GTET_PAD_SMALL);

    gtk_widget_set_hexpand (*content_box, TRUE);
    gtk_widget_set_vexpand (*content_box, TRUE);
    gtk_widget_set_halign (*action_box, GTK_ALIGN_END);
    gtk_widget_set_margin_start (*action_box, GTET_PAD);
    gtk_widget_set_margin_end (*action_box, GTET_PAD);
    gtk_widget_set_margin_top (*action_box, GTET_PAD_SMALL);
    gtk_widget_set_margin_bottom (*action_box, GTET_PAD);

    gtk_box_append (GTK_BOX (root), *content_box);
    gtk_box_append (GTK_BOX (root), *action_box);
    gtk_window_set_child (GTK_WINDOW (window), root);

    return window;
}

extern GtkWidget *app;

/*****************************************************/
/* connecting dialog - a dialog with a cancel button */
/*****************************************************/
static GtkWidget *connectingdialog = 0, *connectdialog;
static GtkWidget *progressbar;
static gint timeouttag = 0;

GtkWidget *team_dialog;

static void
connectingdialog_cancel_clicked (GtkButton *button G_GNUC_UNUSED,
                                 gpointer data G_GNUC_UNUSED)
{
    if (timeouttag != 0) {
        g_source_remove (timeouttag);
        timeouttag = 0;
    }

    if (connectingdialog == NULL)
        return;

    client_disconnect ();
    gtk_window_destroy (GTK_WINDOW (connectingdialog));
    connectingdialog = NULL;
}

gboolean connectingdialog_delete (GtkWindow *window G_GNUC_UNUSED,
                                  gpointer data G_GNUC_UNUSED)
{
    return TRUE; /* don't close while a connection attempt is active */
}

gint connectingdialog_timeout (void)
{
    gtk_progress_bar_pulse (GTK_PROGRESS_BAR (progressbar));
    return TRUE;
}

void connectingdialog_new (void)
{
    GtkWidget *content, *actions, *cancel;

    if (connectingdialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (connectingdialog));
      return;
    }

    connectingdialog = gtet_window_new (_("Connect to server"),
                                        GTK_WINDOW (connectdialog),
                                        TRUE, &content, &actions);
    gtk_window_set_resizable (GTK_WINDOW (connectingdialog), FALSE);

    progressbar = gtk_progress_bar_new ();
    gtk_widget_set_margin_start (progressbar, GTET_PAD);
    gtk_widget_set_margin_end (progressbar, GTET_PAD);
    gtk_widget_set_margin_top (progressbar, GTET_PAD);
    gtk_widget_set_margin_bottom (progressbar, GTET_PAD_SMALL);
    gtk_box_append (GTK_BOX (content), progressbar);

    cancel = gtk_button_new_with_mnemonic (_("_Cancel"));
    gtk_box_append (GTK_BOX (actions), cancel);
    g_signal_connect (cancel, "clicked",
                      G_CALLBACK (connectingdialog_cancel_clicked), NULL);

    timeouttag = g_timeout_add (20, (GSourceFunc)connectingdialog_timeout,
                                NULL);
    g_signal_connect (connectingdialog, "close-request",
                      G_CALLBACK (connectingdialog_delete), NULL);
    gtk_window_present (GTK_WINDOW (connectingdialog));
}

void connectingdialog_destroy (void)
{
    if (timeouttag != 0) g_source_remove (timeouttag);
    timeouttag = 0;
    if (connectingdialog == 0) return;
    gtk_window_destroy (GTK_WINDOW (connectingdialog));
    connectingdialog = 0;
}

/*******************/
/* the team dialog */
/*******************/
void teamdialog_destroy (void)
{
    if (team_dialog != NULL)
        gtk_window_destroy (GTK_WINDOW (team_dialog));
}

static void
teamdialog_destroyed (GtkWidget *widget G_GNUC_UNUSED,
                      gpointer data G_GNUC_UNUSED)
{
    team_dialog = NULL;
}

static void
teamdialog_cancel_clicked (GtkButton *button G_GNUC_UNUSED,
                           gpointer data G_GNUC_UNUSED)
{
    teamdialog_destroy ();
}

static void
teamdialog_ok_clicked (GtkButton *button G_GNUC_UNUSED, gpointer data)
{
    GtkEntry *entry = GTK_ENTRY (data);

    g_settings_set_string (settings, "player-team",
                           gtk_editable_get_text (GTK_EDITABLE (entry)));
    tetrinet_changeteam (gtk_editable_get_text (GTK_EDITABLE (entry)));
    teamdialog_destroy ();
}

void teamdialog_new (void)
{
    GtkWidget *content, *actions, *cancel, *ok;
    GtkWidget *hbox, *widget, *entry;
    gchar *team_utf8 = team;
  
    if (team_dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (team_dialog));
      return;
    }

    team_dialog = gtet_window_new (_("Change team"), GTK_WINDOW (app),
                                   FALSE, &content, &actions);
    gtk_window_set_resizable (GTK_WINDOW (team_dialog), FALSE);

    /* entry and label */
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GTET_PAD_SMALL);
    widget = gtk_label_new (_("Team name:"));
    gtk_box_append (GTK_BOX (hbox), widget);
    entry = gtk_entry_new_with_buffer (gtk_entry_buffer_new("Team", 4));
    gtk_editable_set_text (GTK_EDITABLE (entry), team_utf8);
    g_object_set ((GObject*)entry, "activates_default", TRUE, NULL);
    gtk_box_append (GTK_BOX (hbox), entry);
    gtk_widget_set_margin_start (hbox, GTET_PAD_SMALL);
    gtk_widget_set_margin_end (hbox, GTET_PAD_SMALL);
    gtk_widget_set_margin_top (hbox, GTET_PAD_SMALL);
    gtk_widget_set_margin_bottom (hbox, GTET_PAD_SMALL);
    gtk_box_append (GTK_BOX (content), hbox);

    cancel = gtk_button_new_with_mnemonic (_("_Cancel"));
    ok = gtk_button_new_with_mnemonic (_("_OK"));
    gtk_box_append (GTK_BOX (actions), cancel);
    gtk_box_append (GTK_BOX (actions), ok);
    gtk_window_set_default_widget (GTK_WINDOW (team_dialog), ok);

    g_signal_connect (cancel, "clicked",
                      G_CALLBACK (teamdialog_cancel_clicked), NULL);
    g_signal_connect (ok, "clicked",
                      G_CALLBACK (teamdialog_ok_clicked), entry);
    g_signal_connect (team_dialog, "destroy",
                      G_CALLBACK (teamdialog_destroyed), NULL);
    gtk_window_present (GTK_WINDOW (team_dialog));
}

/**********************/
/* the connect dialog */
/**********************/
static int connecting;
static GtkWidget *serveraddressentry, *nicknameentry, *teamnameentry, *spectatorcheck, *passwordentry;
static GtkWidget *passwordlabel, *teamnamelabel;
static GtkWidget *originalradio, *tetrifastradio;

static void connectdialog_destroy (void);

#ifdef ENABLE_SERVERLIST
static GtkWidget *serverlist_button, *serverlist_popover;
static GtkStringList *serverlist_model;
static SoupSession *serverlist_session;
static GCancellable *serverlist_cancellable;
#endif


void connectdialog_button (GtkWindow *dialog, gint button)
{
    gchar *nick; /* intermediate buffer for recoding purposes */
    const gchar *server1;

    switch (button) {
    case GTK_RESPONSE_OK:
        /* connect now */
        server1 = gtk_editable_get_text (GTK_EDITABLE (serveraddressentry));
        if (g_utf8_strlen (server1, -1) <= 0)
        {
          gtet_show_error (dialog, _("You must specify a server name."));
          return;
        }

	//spectating = GTK_TOGGLE_BUTTON(spectatorcheck)->active ? TRUE : FALSE;
	spectating = gtk_check_button_get_active (GTK_CHECK_BUTTON (spectatorcheck));

        if (spectating)
        {
          g_utf8_strncpy (specpassword, gtk_editable_get_text (GTK_EDITABLE (passwordentry)),
                          g_utf8_strlen (gtk_editable_get_text (GTK_EDITABLE (passwordentry)), -1));
          if (g_utf8_strlen (specpassword, -1) <= 0)
          {
            gtet_show_error (dialog, _("Please specify a password to connect as spectator."));
            return;
          }
        }
        
        GTET_O_STRCPY (team, gtk_editable_get_text (GTK_EDITABLE (teamnameentry)));
        
        nick = g_strdup (gtk_editable_get_text (GTK_EDITABLE (nicknameentry)));
        g_strstrip (nick); /* we remove leading and trailing whitespaces */
        if (g_utf8_strlen (nick, -1) > 0)
        {
          client_init (server1, nick);
        }
        else
        {
            gtet_show_error (dialog, _("Please specify a valid nickname."));
            return;
        }
        
        g_settings_set_string (settings, "server", server1);
        g_settings_set_string (settings, "player-nickname", nick);
        g_settings_set_string (settings, "player-team",
                                 gtk_editable_get_text (GTK_EDITABLE (teamnameentry)));
        g_settings_set_boolean (settings, "gamemode", gamemode);

        g_free (nick);
        break;
    case GTK_RESPONSE_CANCEL:
        connectdialog_destroy ();
        break;
    }
}

void connectdialog_spectoggle (GtkWidget *widget)
{
    if (gtk_check_button_get_active (GTK_CHECK_BUTTON (widget))) {
        gtk_widget_set_sensitive (passwordentry, TRUE);
        gtk_widget_set_sensitive (passwordlabel, TRUE);
        gtk_widget_set_sensitive (teamnameentry, FALSE);
        gtk_widget_set_sensitive (teamnamelabel, FALSE);
    }
    else {
        gtk_widget_set_sensitive (passwordentry, FALSE);
        gtk_widget_set_sensitive (passwordlabel, FALSE);
        gtk_widget_set_sensitive (teamnameentry, TRUE);
        gtk_widget_set_sensitive (teamnamelabel, TRUE);
    }
}

void connectdialog_originaltoggle (GtkWidget *widget)
{
    if (gtk_check_button_get_active (GTK_CHECK_BUTTON (widget))) {
        gamemode = ORIGINAL;
    }
}

void connectdialog_tetrifasttoggle (GtkWidget *widget)
{
    if (gtk_check_button_get_active (GTK_CHECK_BUTTON (widget))) {
        gamemode = TETRIFAST;
    }
}

#ifdef ENABLE_SERVERLIST

static gboolean
serverlist_contains (const char *name)
{
    guint i, n;

    if (name == NULL || *name == '\0' || serverlist_model == NULL)
        return FALSE;

    n = g_list_model_get_n_items (G_LIST_MODEL (serverlist_model));
    for (i = 0; i < n; i++) {
        const char *item = gtk_string_list_get_string (serverlist_model, i);

        if (g_strcmp0 (item, name) == 0)
            return TRUE;
    }

    return FALSE;
}

static void
serverlist_add_unique (const char *name)
{
    if (name != NULL && *name != '\0' && !serverlist_contains (name))
        gtk_string_list_append (serverlist_model, name);
}

static void
serverlist_reset (void)
{
    const char *current;
    guint n;

    if (serverlist_model == NULL)
        return;

    n = g_list_model_get_n_items (G_LIST_MODEL (serverlist_model));
    if (n != 0)
        gtk_string_list_splice (serverlist_model, 0, n, NULL);

    current = gtk_editable_get_text (GTK_EDITABLE (serveraddressentry));
    serverlist_add_unique (current);
    serverlist_add_unique ("tetrinet.fr");
    serverlist_add_unique ("localhost");
}

static void
serverlist_factory_setup (GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                          GtkListItem *list_item,
                          gpointer data G_GNUC_UNUSED)
{
    GtkWidget *label = gtk_label_new (NULL);

    gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
    gtk_widget_set_margin_start (label, GTET_PAD_SMALL);
    gtk_widget_set_margin_end (label, GTET_PAD_SMALL);
    gtk_widget_set_margin_top (label, 2);
    gtk_widget_set_margin_bottom (label, 2);
    gtk_list_item_set_child (list_item, label);
}

static void
serverlist_factory_bind (GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                         GtkListItem *list_item,
                         gpointer data G_GNUC_UNUSED)
{
    GtkStringObject *item = GTK_STRING_OBJECT (gtk_list_item_get_item (list_item));
    GtkWidget *label = gtk_list_item_get_child (list_item);

    gtk_label_set_text (GTK_LABEL (label), gtk_string_object_get_string (item));
}

static void
serverlist_activated (GtkListView *view G_GNUC_UNUSED,
                      guint position,
                      gpointer data G_GNUC_UNUSED)
{
    const char *server_name;

    if (serverlist_model == NULL)
        return;

    server_name = gtk_string_list_get_string (serverlist_model, position);
    if (server_name == NULL)
        return;

    gtk_editable_set_text (GTK_EDITABLE (serveraddressentry), server_name);
    gtk_editable_set_position (GTK_EDITABLE (serveraddressentry), -1);

    if (serverlist_popover != NULL)
        gtk_popover_popdown (GTK_POPOVER (serverlist_popover));
}

static void
serverlist_xmlparse_open_tag (GMarkupParseContext *context G_GNUC_UNUSED,
                              const gchar *element_name,
                              const gchar **attribute_names,
                              const gchar **attribute_values,
                              gpointer user_data G_GNUC_UNUSED,
                              GError **error G_GNUC_UNUSED)
{
    guint i;

    if (g_strcmp0 (element_name, "server") != 0)
        return;

    for (i = 0; attribute_names[i] != NULL; i++) {
        if (g_strcmp0 (attribute_names[i], "name") == 0) {
            serverlist_add_unique (attribute_values[i]);
            break;
        }
    }
}

static void
serverlist_xmlparse_error (GMarkupParseContext *context G_GNUC_UNUSED,
                           GError *error,
                           gpointer user_data G_GNUC_UNUSED)
{
    g_warning ("Error parsing server list XML: %s", error->message);
}

static const GMarkupParser serverlist_xmlparser = {
    .start_element = serverlist_xmlparse_open_tag,
    .error = serverlist_xmlparse_error
};

static void
connectdialog_receivelist (GObject *source_object,
                           GAsyncResult *result,
                           gpointer user_data)
{
    SoupSession *session = SOUP_SESSION (source_object);
    SoupMessage *message = SOUP_MESSAGE (user_data);
    GBytes *body;
    GError *error = NULL;
    guint status;

    body = soup_session_send_and_read_finish (session, result, &error);

    if (serverlist_button != NULL)
        gtk_widget_set_sensitive (serverlist_button, TRUE);

    if (error != NULL) {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            gchar *msg = g_strdup_printf (_("Could not download the server list:\n%s"),
                                          error->message);
            if (connectdialog != NULL)
                gtet_show_error (GTK_WINDOW (connectdialog), msg);
            g_free (msg);
        }

        g_clear_error (&error);
        g_object_unref (message);
        return;
    }

    status = soup_message_get_status (message);
    if (!SOUP_STATUS_IS_SUCCESSFUL (status)) {
        gchar *uri = g_uri_to_string (soup_message_get_uri (message));
        gchar *msg = g_strdup_printf (_("Could not download the server list from %s "
                                        "(HTTP %u: %s)."),
                                      uri, status, soup_status_get_phrase (status));

        if (connectdialog != NULL)
            gtet_show_error (GTK_WINDOW (connectdialog), msg);

        g_free (msg);
        g_free (uri);
        g_bytes_unref (body);
        g_object_unref (message);
        return;
    }

    if (connectdialog != NULL && serverlist_model != NULL) {
        GMarkupParseContext *ctx;
        gconstpointer data;
        gsize length;

        serverlist_reset ();

        data = g_bytes_get_data (body, &length);
        ctx = g_markup_parse_context_new (&serverlist_xmlparser, 0, NULL, NULL);

        if (g_markup_parse_context_parse (ctx, data, length, &error))
            g_markup_parse_context_end_parse (ctx, &error);

        if (error != NULL) {
            gchar *msg = g_strdup_printf (_("Could not parse the server list:\n%s"),
                                          error->message);
            gtet_show_error (GTK_WINDOW (connectdialog), msg);
            g_free (msg);
            g_clear_error (&error);
        } else if (serverlist_popover != NULL) {
            gtk_popover_popup (GTK_POPOVER (serverlist_popover));
        }

        g_markup_parse_context_free (ctx);
    }

    g_bytes_unref (body);
    g_object_unref (message);
}

static void
connectdialog_getlist (GtkButton *button G_GNUC_UNUSED,
                       gpointer data G_GNUC_UNUSED)
{
    gchar *url;
    GUri *uri;
    GError *error = NULL;
    SoupMessage *message;

    url = g_settings_get_string (settings, "serverlistserver");
    uri = g_uri_parse (url, G_URI_FLAGS_NONE, &error);

    if (uri == NULL || g_uri_get_scheme (uri) == NULL || g_uri_get_host (uri) == NULL) {
        gchar *msg = g_strdup_printf (_("Invalid server-list URL in settings:\n%s"), url);

        gtet_show_error (GTK_WINDOW (connectdialog), msg);
        g_free (msg);
        g_clear_error (&error);
        if (uri != NULL)
            g_uri_unref (uri);
        g_free (url);
        return;
    }

    message = soup_message_new ("GET", url);
    if (message == NULL) {
        gchar *msg = g_strdup_printf (_("Could not create a request for:\n%s"), url);

        gtet_show_error (GTK_WINDOW (connectdialog), msg);
        g_free (msg);
        g_uri_unref (uri);
        g_free (url);
        return;
    }

    if (serverlist_cancellable != NULL) {
        g_cancellable_cancel (serverlist_cancellable);
        g_clear_object (&serverlist_cancellable);
    }

    serverlist_cancellable = g_cancellable_new ();
    gtk_widget_set_sensitive (serverlist_button, FALSE);

    soup_session_send_and_read_async (serverlist_session,
                                      message,
                                      G_PRIORITY_DEFAULT,
                                      serverlist_cancellable,
                                      connectdialog_receivelist,
                                      g_object_ref (message));

    g_object_unref (message);
    g_uri_unref (uri);
    g_free (url);
}

static void
serverlist_setup_widgets (GtkWidget *parent_box)
{
    GtkListItemFactory *factory;
    GtkSelectionModel *selection;
    GtkWidget *list, *scroll;
    gchar *url;
    GUri *uri = NULL;
    GError *error = NULL;
    const char *host = NULL;
    gchar *label;

    serverlist_model = gtk_string_list_new (NULL);
    serverlist_reset ();

    selection = GTK_SELECTION_MODEL (
        gtk_single_selection_new (G_LIST_MODEL (g_object_ref (serverlist_model))));

    factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup",
                      G_CALLBACK (serverlist_factory_setup), NULL);
    g_signal_connect (factory, "bind",
                      G_CALLBACK (serverlist_factory_bind), NULL);

    list = gtk_list_view_new (selection, factory);
    g_signal_connect (list, "activate",
                      G_CALLBACK (serverlist_activated), NULL);

    scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request (scroll, 260, 220);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), list);

    serverlist_popover = gtk_popover_new ();
    gtk_popover_set_child (GTK_POPOVER (serverlist_popover), scroll);

    url = g_settings_get_string (settings, "serverlistserver");
    uri = g_uri_parse (url, G_URI_FLAGS_NONE, &error);
    if (uri != NULL)
        host = g_uri_get_host (uri);

    if (host != NULL)
        label = g_strdup_printf (_("_Get server list from %s"), host);
    else
        label = g_strdup (_("_Get server list"));

    serverlist_button = gtk_button_new_with_mnemonic (label);
    gtk_box_append (GTK_BOX (parent_box), serverlist_button);
    gtk_widget_set_parent (serverlist_popover, serverlist_button);
    g_signal_connect (serverlist_button, "clicked",
                      G_CALLBACK (connectdialog_getlist), NULL);

    g_free (label);
    g_clear_error (&error);
    if (uri != NULL)
        g_uri_unref (uri);
    g_free (url);

    serverlist_session = soup_session_new ();
    g_object_set (serverlist_session,
                  "timeout", 60u,
                  "user-agent", PACKAGE_NAME "/" PACKAGE_VERSION,
                  NULL);
}

static void
serverlist_cleanup (void)
{
    if (serverlist_cancellable != NULL)
        g_cancellable_cancel (serverlist_cancellable);

    g_clear_object (&serverlist_cancellable);
    g_clear_object (&serverlist_session);
    g_clear_object (&serverlist_model);

    serverlist_button = NULL;
    serverlist_popover = NULL;
}

static void serverlist_detach_popover ()
{
    if (serverlist_popover != NULL &&
        gtk_widget_get_parent (serverlist_popover) != NULL)
    {
        gtk_popover_popdown (GTK_POPOVER (serverlist_popover));
        gtk_widget_unparent (serverlist_popover);
    }

    serverlist_popover = NULL;
}
#endif /* ENABLE_SERVERLIST */

static void
connectdialog_destroy (void)
{
    if (connectdialog == NULL)
        return;

#ifdef ENABLE_SERVERLIST
    serverlist_detach_popover ();
#endif

    gtk_window_destroy (GTK_WINDOW (connectdialog));
}

static gboolean
connectdialog_close_request (GtkWindow *window G_GNUC_UNUSED,
                             gpointer data G_GNUC_UNUSED)
{
#ifdef ENABLE_SERVERLIST
    /*
     * The popover is manually parented to serverlist_button, so detach it
     * while the widget hierarchy is still alive.  Returning FALSE lets GTK
     * continue with the normal window close afterwards.
     */
    serverlist_detach_popover ();
#endif

    return FALSE;
}

void connectdialog_connected (void)
{
    connectdialog_destroy ();
}

static void
connectdialog_destroyed (GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;

#ifdef ENABLE_SERVERLIST
    serverlist_cleanup ();
#endif

    connectdialog = NULL;
    connecting = FALSE;
}

static void
connectdialog_cancel_clicked (GtkButton *button G_GNUC_UNUSED, gpointer data)
{
    connectdialog_button (GTK_WINDOW (data), GTK_RESPONSE_CANCEL);
}

static void
connectdialog_ok_clicked (GtkButton *button G_GNUC_UNUSED, gpointer data)
{
    connectdialog_button (GTK_WINDOW (data), GTK_RESPONSE_OK);
}

void connectdialog_new (void)
{
    GtkWidget *content, *actions, *cancel, *ok;
    GtkWidget *widget, *table1, *table2, *frame;

    /* check if dialog is already displayed */
    if (connecting) 
    {
      gtk_window_present (GTK_WINDOW (connectdialog));
      return;
    }
    connecting = TRUE;

    /* make window that asks for address/nickname */
    connectdialog = gtet_window_new (_("Connect to server"), GTK_WINDOW (app),
                                     FALSE, &content, &actions);
    gtk_window_set_resizable (GTK_WINDOW (connectdialog), FALSE);

    cancel = gtk_button_new_with_mnemonic (_("_Cancel"));
    ok = gtk_button_new_with_mnemonic (_("_OK"));
    gtk_box_append (GTK_BOX (actions), cancel);
    gtk_box_append (GTK_BOX (actions), ok);
    gtk_window_set_default_widget (GTK_WINDOW (connectdialog), ok);

    g_signal_connect (cancel, "clicked",
                      G_CALLBACK (connectdialog_cancel_clicked), connectdialog);
    g_signal_connect (ok, "clicked",
                      G_CALLBACK (connectdialog_ok_clicked), connectdialog);
    g_signal_connect (connectdialog, "close-request",
                      G_CALLBACK (connectdialog_close_request), NULL);
    g_signal_connect (connectdialog, "destroy",
                      G_CALLBACK (connectdialog_destroyed), NULL);
    /* main table */
    table1 = gtk_table_new (2, 2, FALSE);
    gtk_table_set_row_spacings (GTK_TABLE(table1), GTET_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table1), GTET_PAD_SMALL);

    /* server address */
    table2 = gtk_table_new (2, 1, FALSE);

    serveraddressentry = gtk_entry_new_with_buffer (gtk_entry_buffer_new("Server", 6));
    g_object_set((GObject*)serveraddressentry,
                 "activates_default", TRUE, NULL);
    gtk_editable_set_text (GTK_EDITABLE (serveraddressentry), server);
    gtk_widget_set_visible (serveraddressentry, TRUE);
    gtk_table_attach (GTK_TABLE(table2), serveraddressentry,
                      0, 1, 0, 1, GTK_FILL | GTK_EXPAND,
                      GTK_FILL | GTK_EXPAND, 0, 0);
    /* game type radio buttons */
    originalradio = gtk_check_button_new_with_mnemonic (_("O_riginal"));
    tetrifastradio = gtk_check_button_new_with_mnemonic (_("Tetri_Fast"));
    gtk_check_button_set_group (GTK_CHECK_BUTTON (tetrifastradio),
                                GTK_CHECK_BUTTON (originalradio));
    switch (gamemode) {
    case ORIGINAL:
        gtk_check_button_set_active (GTK_CHECK_BUTTON (originalradio), TRUE);
        break;
    case TETRIFAST:
        gtk_check_button_set_active (GTK_CHECK_BUTTON (tetrifastradio), TRUE);
        break;
    }
    gtk_widget_set_visible (originalradio, TRUE);
    gtk_widget_set_visible (tetrifastradio, TRUE);
    widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, GTET_PAD_SMALL);
    gtk_box_append (GTK_BOX (widget), originalradio);
    gtk_box_append (GTK_BOX (widget), tetrifastradio);
#ifdef ENABLE_SERVERLIST
    serverlist_setup_widgets (widget);
#endif
    gtk_widget_set_visible (widget, TRUE);
    gtk_table_attach (GTK_TABLE(table2), widget,
                      0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

    gtk_table_set_row_spacings (GTK_TABLE(table2), GTET_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table2), GTET_PAD_SMALL);
    gtk_widget_set_margin_start (table2, GTET_PAD);
    gtk_widget_set_margin_end (table2, GTET_PAD);
    gtk_widget_set_margin_top (table2, GTET_PAD);
    gtk_widget_set_margin_bottom (table2, GTET_PAD);
    gtk_widget_set_visible (table2, TRUE);
    frame = gtk_frame_new (_("Server address"));
    gtk_frame_set_child (GTK_FRAME (frame), table2);
    gtk_widget_set_visible (frame, TRUE);
    gtk_table_attach (GTK_TABLE(table1), frame, 0, 2, 0, 1,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

    /* spectator checkbox + password */
    table2 = gtk_table_new (1, 1, FALSE);

    spectatorcheck = gtk_check_button_new_with_mnemonic (_("Connect as a _spectator"));
    gtk_widget_set_visible (spectatorcheck, TRUE);
    gtk_table_attach (GTK_TABLE(table2), spectatorcheck, 0, 2, 0, 1,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
    passwordlabel = gtk_label_new_with_mnemonic (_("_Password:"));
    gtk_widget_set_visible (passwordlabel, TRUE);
    gtk_table_attach (GTK_TABLE(table2), passwordlabel, 0, 1, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
    passwordentry = gtk_entry_new ();
    gtk_label_set_mnemonic_widget (GTK_LABEL (passwordlabel), passwordentry);
    gtk_entry_set_visibility (GTK_ENTRY(passwordentry), FALSE);
    g_object_set(G_OBJECT(passwordentry),
                 "activates_default", TRUE, NULL);
    gtk_widget_set_visible (passwordentry, TRUE);
    gtk_table_attach (GTK_TABLE(table2), passwordentry, 1, 2, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

    gtk_table_set_row_spacings (GTK_TABLE(table2), GTET_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table2), GTET_PAD_SMALL);
    gtk_widget_set_margin_start (table2, GTET_PAD);
    gtk_widget_set_margin_end (table2, GTET_PAD);
    gtk_widget_set_margin_top (table2, GTET_PAD);
    gtk_widget_set_margin_bottom (table2, GTET_PAD);
    gtk_widget_set_visible (table2, TRUE);
    frame = gtk_frame_new (_("Spectate game"));
    gtk_frame_set_child (GTK_FRAME (frame), table2);
    gtk_widget_set_visible (frame, TRUE);
    gtk_table_attach (GTK_TABLE(table1), frame, 0, 1, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

    /* nickname and teamname entries */
    table2 = gtk_table_new (1, 1, FALSE);

    widget = gtk_label_new_with_mnemonic (_("_Nick name:"));
    gtk_widget_set_visible (widget, TRUE);
    gtk_table_attach (GTK_TABLE(table2), widget, 0, 1, 0, 1,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);
    nicknameentry = gtk_entry_new_with_buffer (gtk_entry_buffer_new("Nickname",8));
    gtk_label_set_mnemonic_widget (GTK_LABEL (widget), nicknameentry);
    g_object_set((GObject*)nicknameentry, "activates_default", TRUE, NULL);
    gtk_editable_set_text (GTK_EDITABLE (nicknameentry), nick);
    /* g_free (aux);*/
    gtk_widget_set_visible (nicknameentry, TRUE);
    gtk_table_attach (GTK_TABLE(table2), nicknameentry, 1, 2, 0, 1,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);
    teamnamelabel = gtk_label_new_with_mnemonic (_("_Team name:"));
    gtk_widget_set_visible (teamnamelabel, TRUE);
    gtk_table_attach (GTK_TABLE(table2), teamnamelabel, 0, 1, 1, 2,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);
    teamnameentry = gtk_entry_new_with_buffer (gtk_entry_buffer_new("Teamname", 8));
    gtk_label_set_mnemonic_widget (GTK_LABEL (teamnamelabel), teamnameentry);
    g_object_set((GObject*)teamnameentry, "activates_default", TRUE, NULL);
    gtk_editable_set_text (GTK_EDITABLE (teamnameentry), team);
    /*g_free (aux);*/
    gtk_widget_set_visible (teamnameentry, TRUE);
    gtk_table_attach (GTK_TABLE(table2), teamnameentry, 1, 2, 1, 2,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);

    gtk_table_set_row_spacings (GTK_TABLE(table2), GTET_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table2), GTET_PAD_SMALL);
    gtk_widget_set_margin_start (table2, GTET_PAD);
    gtk_widget_set_margin_end (table2, GTET_PAD);
    gtk_widget_set_margin_top (table2, GTET_PAD);
    gtk_widget_set_margin_bottom (table2, GTET_PAD);
    gtk_widget_set_visible (table2, TRUE);
    frame = gtk_frame_new (_("Player information"));
    gtk_frame_set_child (GTK_FRAME (frame), table2);
    gtk_widget_set_visible (frame, TRUE);
    gtk_table_attach (GTK_TABLE(table1), frame, 1, 2, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

    gtk_widget_set_visible (table1, TRUE);

    gtk_widget_set_margin_start (table1, GTET_PAD_SMALL);
    gtk_widget_set_margin_end (table1, GTET_PAD_SMALL);
    gtk_widget_set_margin_top (table1, GTET_PAD_SMALL);
    gtk_widget_set_margin_bottom (table1, GTET_PAD_SMALL);
    gtk_box_append (GTK_BOX (content), table1);

    gtk_check_button_set_active (GTK_CHECK_BUTTON (spectatorcheck), spectating);
    connectdialog_spectoggle (spectatorcheck);
    g_signal_connect (G_OBJECT(spectatorcheck), "toggled",
                        G_CALLBACK(connectdialog_spectoggle), NULL);
    g_signal_connect (G_OBJECT(originalradio), "toggled",
                        G_CALLBACK(connectdialog_originaltoggle), NULL);
    g_signal_connect (G_OBJECT(tetrifastradio), "toggled",
                        G_CALLBACK(connectdialog_tetrifasttoggle), NULL);
    gtk_window_present (GTK_WINDOW (connectdialog));
}

GtkWidget *prefdialog;

/*************************/
/* the change key dialog */
/*************************/

static GMainLoop *key_dialog_loop;
static gint key_dialog_key;

static gboolean
key_dialog_callback (GtkEventControllerKey *controller G_GNUC_UNUSED,
                     guint keyval,
                     guint keycode G_GNUC_UNUSED,
                     GdkModifierType state G_GNUC_UNUSED,
                     gpointer data)
{
    GtkWindow *window = GTK_WINDOW (data);

    key_dialog_key = gdk_keyval_to_lower (keyval);
    if (key_dialog_loop != NULL)
        g_main_loop_quit (key_dialog_loop);
    return TRUE;
}

static gboolean
key_dialog_close (GtkWindow *window G_GNUC_UNUSED,
                  gpointer data G_GNUC_UNUSED)
{
    key_dialog_key = 0;
    if (key_dialog_loop != NULL)
        g_main_loop_quit (key_dialog_loop);
    return TRUE;
}

gint key_dialog (char *msg)
{
    GtkWidget *dialog, *label;
    GtkEventController *controller;

    dialog = gtk_window_new ();
    gtk_window_set_title (GTK_WINDOW (dialog), _("Change Key"));
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (prefdialog));
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

    label = gtk_label_new (msg);
    gtk_widget_set_margin_start (label, GTET_PAD);
    gtk_widget_set_margin_end (label, GTET_PAD);
    gtk_widget_set_margin_top (label, GTET_PAD);
    gtk_widget_set_margin_bottom (label, GTET_PAD);
    gtk_window_set_child (GTK_WINDOW (dialog), label);

    controller = gtk_event_controller_key_new ();
    g_signal_connect (controller, "key-pressed",
                      G_CALLBACK (key_dialog_callback), dialog);
    gtk_widget_add_controller (dialog, controller);
    g_signal_connect (dialog, "close-request",
                      G_CALLBACK (key_dialog_close), NULL);

    key_dialog_key = 0;
    key_dialog_loop = g_main_loop_new (NULL, FALSE);
    gtk_window_present (GTK_WINDOW (dialog));
    g_main_loop_run (key_dialog_loop);
    g_main_loop_unref (key_dialog_loop);
    key_dialog_loop = NULL;
    gtk_window_destroy (GTK_WINDOW (dialog));

    return key_dialog_key;
}

/**************************/
/* the preferences dialog */
/**************************/
GtkWidget *themelist, *keyclist;
GtkWidget *timestampcheck;
GtkWidget *soundcheck;
GtkWidget *namelabel, *authlabel, *desclabel;

gchar *actions[K_NUM];

struct themelistentry {
    char dir[1024];
    char name[1024];
} themes[64];

int themecount;
int theme_select;

typedef struct _ThemeListItem {
    GObject parent_instance;
    char *name;
    int theme_index;
} ThemeListItem;

typedef struct _ThemeListItemClass {
    GObjectClass parent_class;
} ThemeListItemClass;

typedef struct _KeyListItem {
    GObject parent_instance;
    char *action;
    char *key_name;
    int key_index;
    char *settings_key;
} KeyListItem;

typedef struct _KeyListItemClass {
    GObjectClass parent_class;
} KeyListItemClass;

G_DEFINE_TYPE (ThemeListItem, theme_list_item, G_TYPE_OBJECT)
G_DEFINE_TYPE (KeyListItem, key_list_item, G_TYPE_OBJECT)

static GListStore *theme_store;
static GListStore *keys_store;
static GtkSingleSelection *theme_selection;
static GtkSingleSelection *key_selection;

static void
theme_list_item_finalize (GObject *object)
{
    ThemeListItem *item = (ThemeListItem *) object;

    g_free (item->name);
    G_OBJECT_CLASS (theme_list_item_parent_class)->finalize (object);
}

static void
theme_list_item_class_init (ThemeListItemClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = theme_list_item_finalize;
}

static void
theme_list_item_init (ThemeListItem *item)
{
    item->name = NULL;
    item->theme_index = 0;
}

static ThemeListItem *
theme_list_item_new (const char *name, int theme_index)
{
    ThemeListItem *item;

    item = g_object_new (theme_list_item_get_type (), NULL);
    item->name = g_strdup (name);
    item->theme_index = theme_index;

    return item;
}

static void
key_list_item_finalize (GObject *object)
{
    KeyListItem *item = (KeyListItem *) object;

    g_free (item->action);
    g_free (item->key_name);
    g_free (item->settings_key);
    G_OBJECT_CLASS (key_list_item_parent_class)->finalize (object);
}

static void
key_list_item_class_init (KeyListItemClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = key_list_item_finalize;
}

static void
key_list_item_init (KeyListItem *item)
{
    item->action = NULL;
    item->key_name = NULL;
    item->key_index = 0;
    item->settings_key = NULL;
}

static KeyListItem *
key_list_item_new (const char *action,
                   const char *key_name,
                   int key_index,
                   const char *settings_key)
{
    KeyListItem *item;

    item = g_object_new (key_list_item_get_type (), NULL);
    item->action = g_strdup (action);
    item->key_name = g_strdup (key_name);
    item->key_index = key_index;
    item->settings_key = g_strdup (settings_key);

    return item;
}

static void
list_label_setup (GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                  GtkListItem *list_item,
                  gpointer data G_GNUC_UNUSED)
{
    GtkWidget *label = gtk_label_new (NULL);

    gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
    gtk_list_item_set_child (list_item, label);
}

static void
theme_label_bind (GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                  GtkListItem *list_item,
                  gpointer data G_GNUC_UNUSED)
{
    ThemeListItem *item = gtk_list_item_get_item (list_item);
    GtkWidget *label = gtk_list_item_get_child (list_item);

    gtk_label_set_text (GTK_LABEL (label), item->name);
}

enum {
    KEY_COLUMN_ACTION,
    KEY_COLUMN_KEY
};

static void
key_label_bind (GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                GtkListItem *list_item,
                gpointer data)
{
    KeyListItem *item = gtk_list_item_get_item (list_item);
    GtkWidget *label = gtk_list_item_get_child (list_item);
    int column = GPOINTER_TO_INT (data);

    gtk_label_set_text (GTK_LABEL (label),
                        column == KEY_COLUMN_ACTION ? item->action : item->key_name);
}

static GtkColumnViewColumn *
key_column_new (const char *title, int column_id)
{
    GtkListItemFactory *factory;

    factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup", G_CALLBACK (list_label_setup), NULL);
    g_signal_connect (factory, "bind", G_CALLBACK (key_label_bind),
                      GINT_TO_POINTER (column_id));

    /* gtk_column_view_column_new() takes ownership of factory. */
    return gtk_column_view_column_new (title, factory);
}

static void
prefdialog_destroyed (GtkWidget *widget, gpointer data)
{
    (void)widget;
    (void)data;

    prefdialog = NULL;
    themelist = NULL;
    keyclist = NULL;
    theme_selection = NULL;
    key_selection = NULL;
    theme_store = NULL;
    keys_store = NULL;
}

void prefdialog_destroy (void)
{
    if (prefdialog != NULL) {
        gtk_window_destroy (GTK_WINDOW (prefdialog));
    }
}

void prefdialog_drawkeys (void)
{
    static const char *gconf_keys[K_NUM] = {
        [K_RIGHT]    = "right",
        [K_LEFT]     = "left",
        [K_DOWN]     = "down",
        [K_ROTRIGHT] = "rotate-right",
        [K_ROTLEFT]  = "rotate-left",
        [K_DROP]     = "drop",
        [K_DISCARD]  = "discard",
        [K_GAMEMSG]  = "message",
        [K_SPECIAL1] = "special1",
        [K_SPECIAL2] = "special2",
        [K_SPECIAL3] = "special3",
        [K_SPECIAL4] = "special4",
        [K_SPECIAL5] = "special5",
        [K_SPECIAL6] = "special6",
        [K_SPECIAL_SELF] = "special-self"
    };
    int i;

    actions[K_RIGHT]    = _("Move right");
    actions[K_LEFT]     = _("Move left");
    actions[K_DOWN]     = _("Move down");
    actions[K_ROTRIGHT] = _("Rotate right");
    actions[K_ROTLEFT]  = _("Rotate left");
    actions[K_DROP]     = _("Drop piece");
    actions[K_DISCARD]  = _("Discard special");
    actions[K_GAMEMSG]  = _("Send message");
    actions[K_SPECIAL1] = _("Special to field 1");
    actions[K_SPECIAL2] = _("Special to field 2");
    actions[K_SPECIAL3] = _("Special to field 3");
    actions[K_SPECIAL4] = _("Special to field 4");
    actions[K_SPECIAL5] = _("Special to field 5");
    actions[K_SPECIAL6] = _("Special to field 6");
    actions[K_SPECIAL_SELF] = _("Special to self");

    g_list_store_remove_all (keys_store);

    for (i = 0; i < K_NUM; i++) {
        KeyListItem *item;

        item = key_list_item_new (actions[i],
                                  gdk_keyval_name (keys[i]),
                                  i,
                                  gconf_keys[i]);
        g_list_store_append (keys_store, item);
        g_object_unref (item);
    }
}

void prefdialog_restorekeys (void)
{
    guint pos, count;

    count = g_list_model_get_n_items (G_LIST_MODEL (keys_store));

    for (pos = 0; pos < count; pos++) {
        KeyListItem *old_item;
        KeyListItem *new_item;
        gpointer replacement[1];

        old_item = g_list_model_get_item (G_LIST_MODEL (keys_store), pos);
        if (old_item == NULL)
            continue;

        new_item = key_list_item_new (old_item->action,
                                      gdk_keyval_name (defaultkeys[old_item->key_index]),
                                      old_item->key_index,
                                      old_item->settings_key);
        replacement[0] = new_item;
        g_list_store_splice (keys_store, pos, 1, replacement, 1);

        g_settings_set_string (settings_keys,
                               old_item->settings_key,
                               gdk_keyval_name (defaultkeys[old_item->key_index]));

        g_object_unref (new_item);
        g_object_unref (old_item);
    }
}

void prefdialog_changekey (void)
{
    gchar buf[256];
    guint selected;
    gint k;
    KeyListItem *old_item;

    selected = gtk_single_selection_get_selected (key_selection);
    if (selected == GTK_INVALID_LIST_POSITION)
        return;

    old_item = g_list_model_get_item (G_LIST_MODEL (keys_store), selected);
    if (old_item == NULL)
        return;

    g_snprintf (buf, sizeof(buf), (_("Press new key for \"%s\"")), old_item->action);
    k = key_dialog (buf);

    if (k) {
        KeyListItem *new_item;
        gpointer replacement[1];

        new_item = key_list_item_new (old_item->action,
                                      gdk_keyval_name (k),
                                      old_item->key_index,
                                      old_item->settings_key);
        replacement[0] = new_item;
        g_list_store_splice (keys_store, selected, 1, replacement, 1);

        g_settings_set_string (settings_keys,
                               old_item->settings_key,
                               gdk_keyval_name (k));

        g_object_unref (new_item);
    }

    g_object_unref (old_item);
}

void prefdialog_soundtoggle (GtkWidget *check)
{
    gboolean enabled;
    enabled = gtk_check_button_get_active (GTK_CHECK_BUTTON (check));

    g_settings_set_boolean (settings, "sound-enable", enabled);
}

void prefdialog_channeltoggle (GtkWidget *check)
{
    gboolean enabled;
    enabled = gtk_check_button_get_active (GTK_CHECK_BUTTON (check));

    g_settings_set_boolean (settings, "partyline-enable-channel-list", enabled);
}

void prefdialog_timestampstoggle (GtkWidget *check)
{
    gboolean enabled;
    enabled = gtk_check_button_get_active (GTK_CHECK_BUTTON (check));

    g_settings_set_boolean (settings, "partyline-enable-timestamps", enabled);
}

void prefdialog_themelistselect (int n)
{
    char author[1024], desc[1024];

    /* update theme description */
    config_getthemeinfo (themes[n].dir, NULL, author, desc);
    leftlabel_set (namelabel, themes[n].name);
    leftlabel_set (authlabel, author);
    leftlabel_set (desclabel, desc);
  
    g_settings_set_string (settings_themes, "directory", themes[n].dir);
}

static void
prefdialog_themeselect (GtkSingleSelection *selection,
                        GParamSpec *pspec G_GNUC_UNUSED,
                        gpointer data G_GNUC_UNUSED)
{
    guint selected;
    ThemeListItem *item;

    selected = gtk_single_selection_get_selected (selection);
    if (selected == GTK_INVALID_LIST_POSITION)
        return;

    item = g_list_model_get_item (G_LIST_MODEL (theme_store), selected);
    if (item == NULL)
        return;

    prefdialog_themelistselect (item->theme_index);
    g_object_unref (item);
}

static int themelistcomp (const void *a1, const void *b1)
{
    const struct themelistentry *a = a1, *b = b1;
    return strcmp (a->name, b->name);
}

void prefdialog_themelist ()
{
    DIR *d;
    struct dirent *de;
    char str[1024], buf[1024];
    gchar *dir;
    int i;
    int selected_position = -1;
    char *basedir[2];

    dir = g_build_filename (getenv ("HOME"), ".gtetrinet", "themes", NULL);

    basedir[0] = dir; /* load users themes first ... in case we run out */
    basedir[1] = GTETRINET_THEMES;

    themecount = 0;

    for (i = 0; i < 2; i ++) {
        d = opendir (basedir[i]);
        if (d) {
            while ((de = readdir(d))) {
                GTET_O_STRCPY (buf, basedir[i]);
                GTET_O_STRCAT (buf, "/");
                GTET_O_STRCAT (buf, de->d_name);
                GTET_O_STRCAT (buf, "/");

                if (config_getthemeinfo(buf, str, NULL, NULL) == 0) {
                    GTET_O_STRCPY (themes[themecount].dir, buf);
                    GTET_O_STRCPY (themes[themecount].name, str);
                    themecount ++;
                    if (themecount == (sizeof(themes) / sizeof(themes[0])))
                    { /* FIXME: should be dynamic */
                      g_warning("Too many theme files.\n");
                      closedir (d);
                      goto too_many_themes;
                    }
                }
            }
            closedir (d);
        }
    }
    g_free (dir);
 too_many_themes:
    qsort (themes, themecount, sizeof(struct themelistentry), themelistcomp);

    theme_select = 0;
    g_list_store_remove_all (theme_store);

    for (i = 0; i < themecount; i++) {
        ThemeListItem *item;

        item = theme_list_item_new (themes[i].name, i);
        g_list_store_append (theme_store, item);
        g_object_unref (item);

        if (strcmp (themes[i].dir, currenttheme->str) == 0) {
            selected_position = i;
            theme_select = i;
        }
    }

    if (selected_position >= 0) {
        gtk_single_selection_set_selected (theme_selection, selected_position);
        prefdialog_themelistselect (theme_select);
    }
}

static void
prefdialog_close_clicked (GtkButton *button G_GNUC_UNUSED,
                          gpointer data G_GNUC_UNUSED)
{
    prefdialog_destroy ();
}

void prefdialog_new (void)
{
    GtkWidget *content, *actions, *close;
    GtkWidget *label, *table, *frame, *button, *button1, *widget, *table1, *divider, *notebook;
    GtkWidget *themelist_scroll, *key_scroll, *url;
    GtkWidget *channel_list_check;
    GtkListItemFactory *theme_factory;
    GtkColumnViewColumn *key_column;
  
    if (prefdialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (prefdialog));
      return;
    }

    prefdialog = gtet_window_new (_("GTetrinet Preferences"), GTK_WINDOW (app),
                                  FALSE, &content, &actions);
    gtk_window_set_destroy_with_parent (GTK_WINDOW (prefdialog), TRUE);

    close = gtk_button_new_with_mnemonic (_("_Close"));
    gtk_box_append (GTK_BOX (actions), close);
    g_signal_connect (close, "clicked",
                      G_CALLBACK (prefdialog_close_clicked), NULL);

    notebook = gtk_notebook_new ();
    gtk_window_set_default_size (GTK_WINDOW (prefdialog), 440, 520);
    gtk_window_set_resizable (GTK_WINDOW (prefdialog), FALSE);

    /* themes */
    theme_store = g_list_store_new (theme_list_item_get_type ());
    theme_selection = gtk_single_selection_new (
        G_LIST_MODEL (theme_store));

    theme_factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (theme_factory, "setup",
                      G_CALLBACK (list_label_setup), NULL);
    g_signal_connect (theme_factory, "bind",
                      G_CALLBACK (theme_label_bind), NULL);

    /* gtk_list_view_new() takes ownership of the selection model and factory. */
    themelist = gtk_list_view_new (GTK_SELECTION_MODEL (theme_selection),
                                   theme_factory);

    themelist_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (themelist_scroll),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (themelist_scroll), themelist);
    gtk_widget_set_size_request (themelist, 160, 200);

    label = leftlabel_new (_("Select a theme from the list.\n"
                             "Install new themes in ~/.gtetrinet/themes/"));

    table1 = gtk_table_new (3, 2, FALSE);
    gtk_widget_set_margin_start (table1, GTET_PAD_SMALL);
    gtk_widget_set_margin_end (table1, GTET_PAD_SMALL);
    gtk_widget_set_margin_top (table1, GTET_PAD_SMALL);
    gtk_widget_set_margin_bottom (table1, GTET_PAD_SMALL);
    gtk_table_set_row_spacings (GTK_TABLE(table1), 0);
    gtk_table_set_col_spacings (GTK_TABLE(table1), GTET_PAD_SMALL);
    widget = leftlabel_new (_("Name:"));
    gtk_table_attach (GTK_TABLE(table1), widget, 0, 1, 0, 1,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    widget = leftlabel_new (_("Author:"));
    gtk_table_attach (GTK_TABLE(table1), widget, 0, 1, 1, 2,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    widget = leftlabel_new (_("Description:"));
    gtk_table_attach (GTK_TABLE(table1), widget, 0, 1, 2, 3,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    namelabel = leftlabel_new ("");
    gtk_table_attach (GTK_TABLE(table1), namelabel, 1, 2, 0, 1,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    authlabel = leftlabel_new ("");
    gtk_table_attach (GTK_TABLE(table1), authlabel, 1, 2, 1, 2,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    desclabel = leftlabel_new ("");
    gtk_table_attach (GTK_TABLE(table1), desclabel, 1, 2, 2, 3,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

    frame = gtk_frame_new (_("Selected Theme"));
        gtk_widget_set_margin_start (frame, GTET_PAD_SMALL);
    gtk_widget_set_margin_end (frame, GTET_PAD_SMALL);
    gtk_widget_set_margin_top (frame, GTET_PAD_SMALL);
    gtk_widget_set_margin_bottom (frame, GTET_PAD_SMALL);
    gtk_widget_set_size_request (frame, 240, 100);
    gtk_frame_set_child (GTK_FRAME (frame), table1);
    
    table = gtk_table_new (3, 2, FALSE);
    gtk_widget_set_margin_start (table, GTET_PAD);
    gtk_widget_set_margin_end (table, GTET_PAD);
    gtk_widget_set_margin_top (table, GTET_PAD);
    gtk_widget_set_margin_bottom (table, GTET_PAD);
    gtk_table_set_row_spacings (GTK_TABLE(table), GTET_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table), GTET_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(table), themelist_scroll, 0, 1, 0, 3,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE(table), label, 1, 2, 0, 1,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE(table), frame, 1, 2, 1, 2,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    url = gtk_link_button_new_with_label ("http://gtetrinet.sourceforge.net/themes.html", _("Download new themes"));
    gtk_table_attach (GTK_TABLE(table), url, 1, 2, 2, 3,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_widget_set_visible (table, TRUE);

    label = gtk_label_new (_("Themes"));
    gtk_widget_set_visible (label, TRUE);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, label);

    /* partyline */
    timestampcheck = gtk_check_button_new_with_mnemonic (_("Enable _Timestamps"));
    gtk_widget_set_visible (timestampcheck, TRUE);
    channel_list_check = gtk_check_button_new_with_mnemonic (_("Enable Channel _List"));
    gtk_widget_set_visible (channel_list_check, TRUE);

    frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append (GTK_BOX (frame), timestampcheck);
    gtk_box_append (GTK_BOX (frame), channel_list_check);
    gtk_widget_set_visible (frame, TRUE);

    gtk_check_button_set_active (GTK_CHECK_BUTTON (timestampcheck),
                                 timestampsenable);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (channel_list_check),
                                 list_enabled);

    g_signal_connect (G_OBJECT(timestampcheck), "toggled",
                      G_CALLBACK(prefdialog_timestampstoggle), NULL);
    g_signal_connect (G_OBJECT (channel_list_check), "toggled",
		      G_CALLBACK (prefdialog_channeltoggle), NULL);

    table = gtk_table_new (3, 1, FALSE);
    gtk_widget_set_margin_start (table, GTET_PAD);
    gtk_widget_set_margin_end (table, GTET_PAD);
    gtk_widget_set_margin_top (table, GTET_PAD);
    gtk_widget_set_margin_bottom (table, GTET_PAD);
    gtk_table_set_row_spacings (GTK_TABLE(table), GTET_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table), GTET_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(table), frame, 0, 1, 0, 1,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_widget_set_visible (table, TRUE);

    label = gtk_label_new (_("Partyline"));
    gtk_widget_set_visible (label, TRUE);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, label);

    /* keyboard */
    keys_store = g_list_store_new (key_list_item_get_type ());
    key_selection = gtk_single_selection_new (
        G_LIST_MODEL (keys_store));

    /* gtk_column_view_new() takes ownership of the selection model. */
    keyclist = gtk_column_view_new (GTK_SELECTION_MODEL (key_selection));

    key_column = key_column_new (_("Action"), KEY_COLUMN_ACTION);
    gtk_column_view_column_set_expand (key_column, TRUE);
    gtk_column_view_append_column (GTK_COLUMN_VIEW (keyclist), key_column);
    g_object_unref (key_column);

    key_column = key_column_new (_("Key"), KEY_COLUMN_KEY);
    gtk_column_view_append_column (GTK_COLUMN_VIEW (keyclist), key_column);
    g_object_unref (key_column);

    key_scroll = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (key_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (key_scroll), keyclist);

    gtk_widget_set_size_request (key_scroll, 240, 200);
    gtk_widget_set_visible (key_scroll, TRUE);

    label = gtk_label_new (_("Select an action from the list and press Change "
                             "Key to change the key associated with the action."));
    gtk_label_set_justify (GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_label_set_wrap (GTK_LABEL(label), TRUE);
    gtk_widget_set_visible (label, TRUE);
    gtk_widget_set_size_request (label, 180, 100);

    button = gtk_button_new_with_mnemonic (_("Change _key..."));
    g_signal_connect (G_OBJECT(button), "clicked",
                      G_CALLBACK (prefdialog_changekey), NULL);
    gtk_widget_set_visible (button, TRUE);

    button1 = gtk_button_new_with_mnemonic (_("_Restore defaults"));
    g_signal_connect (G_OBJECT(button1), "clicked",
                      G_CALLBACK (prefdialog_restorekeys), NULL);
    gtk_widget_set_visible (button1, TRUE);

    table = gtk_table_new (2, 2, FALSE);
    gtk_widget_set_margin_start (table, GTET_PAD);
    gtk_widget_set_margin_end (table, GTET_PAD);
    gtk_widget_set_margin_top (table, GTET_PAD);
    gtk_widget_set_margin_bottom (table, GTET_PAD);
    gtk_table_set_row_spacings (GTK_TABLE(table), GTET_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table), GTET_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(table), key_scroll, 0, 1, 0, 2,
                      GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE(table), label, 1, 2, 0, 1,
                      GTK_FILL, 0, 0, 0);
    frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, GTET_PAD_SMALL);
    gtk_box_append (GTK_BOX (frame), button1);
    gtk_box_append (GTK_BOX (frame), button);
    gtk_widget_set_visible (frame, TRUE);
    gtk_table_attach (GTK_TABLE(table), frame, 1, 2, 1, 2,
                      GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_visible (table, TRUE);

    label = gtk_label_new (_("Keyboard"));
    gtk_widget_set_visible (label, TRUE);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, label);

    /* sound */
    soundcheck = gtk_check_button_new_with_mnemonic (_("Enable _Sound"));
    gtk_widget_set_visible (soundcheck, TRUE);

    frame = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append (GTK_BOX (frame), soundcheck);
    gtk_widget_set_visible (frame, TRUE);

    divider = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_visible (divider, TRUE);

    table = gtk_table_new (3, 1, FALSE);
    gtk_widget_set_margin_start (table, GTET_PAD);
    gtk_widget_set_margin_end (table, GTET_PAD);
    gtk_widget_set_margin_top (table, GTET_PAD);
    gtk_widget_set_margin_bottom (table, GTET_PAD);
    gtk_table_set_row_spacings (GTK_TABLE(table), GTET_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table), GTET_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(table), frame, 0, 1, 0, 1,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_attach (GTK_TABLE(table), divider, 0, 1, 1, 2,
                      GTK_EXPAND | GTK_FILL, 0, 0, GTET_PAD_SMALL);
    gtk_widget_set_visible (table, TRUE);

    label = gtk_label_new (_("Sound"));
    gtk_widget_set_visible (label, TRUE);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, label);

    /* init stuff */
    prefdialog_themelist ();

    prefdialog_drawkeys ();

    gtk_check_button_set_active (GTK_CHECK_BUTTON (soundcheck), soundenable);

    gtk_box_append (GTK_BOX (content), notebook);

    g_signal_connect (G_OBJECT(soundcheck), "toggled",
                      G_CALLBACK(prefdialog_soundtoggle), NULL);
    g_signal_connect (theme_selection, "notify::selected",
                      G_CALLBACK (prefdialog_themeselect), NULL);
    g_signal_connect (G_OBJECT(prefdialog), "destroy",
                      G_CALLBACK(prefdialog_destroyed), NULL);
    gtk_window_present (GTK_WINDOW (prefdialog));
}
