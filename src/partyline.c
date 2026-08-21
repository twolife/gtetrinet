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
#include <gobject/gtype.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "client.h"
#include "tetrinet.h"
#include "partyline.h"
#include "misc.h"

int timestampsenable;
gboolean list_enabled;

/* widgets that we have to do stuff with */
static GtkWidget *playerlist, *textbox, *entrybox,
    *namelabel, *teamlabel, *infolabel, *channel_box,
    *textboxlabel, *channel_list;

static GListStore *work_model, *channellist_model, *playerlist_model;
static GtkSingleSelection *channel_selection;

/* stuff for pline history */
#define PLHSIZE 64
char plhistory[PLHSIZE][256];
int plh_start = 0, plh_end = 0, plh_cur = 0;

/* function prototypes for callbacks */
static void textentry (GtkWidget *widget);
static gboolean entrykey (GtkEventControllerKey *controller,
                           guint keyval,
                           guint keycode,
                           GdkModifierType state,
                           gpointer user_data);
static void channel_activated (GtkColumnView *view, guint position, gpointer data);

typedef struct _PartylinePlayer {
    GObject parent_instance;
    char *number;
    char *name;
    char *team;
} PartylinePlayer;

typedef struct _PartylinePlayerClass {
    GObjectClass parent_class;
} PartylinePlayerClass;

typedef struct _PartylineChannel {
    GObject parent_instance;
    gint number;
    char *name;
    char *players;
    char *state;
    char *description;
} PartylineChannel;

typedef struct _PartylineChannelClass {
    GObjectClass parent_class;
} PartylineChannelClass;

G_DEFINE_TYPE (PartylinePlayer, partyline_player, G_TYPE_OBJECT)
G_DEFINE_TYPE (PartylineChannel, partyline_channel, G_TYPE_OBJECT)

static void
partyline_player_finalize (GObject *object)
{
    PartylinePlayer *item = (PartylinePlayer *) object;

    g_free (item->number);
    g_free (item->name);
    g_free (item->team);
    G_OBJECT_CLASS (partyline_player_parent_class)->finalize (object);
}

static void
partyline_player_class_init (PartylinePlayerClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = partyline_player_finalize;
}

static void
partyline_player_init (PartylinePlayer *item)
{
    item->number = NULL;
    item->name = NULL;
    item->team = NULL;
}

static PartylinePlayer *
partyline_player_new (const char *number, const char *name, const char *team)
{
    PartylinePlayer *item;

    item = g_object_new (partyline_player_get_type (), NULL);
    item->number = g_strdup (number);
    item->name = g_strdup (name);
    item->team = g_strdup (team);

    return item;
}

static void
partyline_channel_finalize (GObject *object)
{
    PartylineChannel *item = (PartylineChannel *) object;

    g_free (item->name);
    g_free (item->players);
    g_free (item->state);
    g_free (item->description);
    G_OBJECT_CLASS (partyline_channel_parent_class)->finalize (object);
}

static void
partyline_channel_class_init (PartylineChannelClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = partyline_channel_finalize;
}

static void
partyline_channel_init (PartylineChannel *item)
{
    item->number = 0;
    item->name = NULL;
    item->players = NULL;
    item->state = NULL;
    item->description = NULL;
}

static PartylineChannel *
partyline_channel_new (gint number,
                       const char *name,
                       const char *players,
                       const char *state,
                       const char *description)
{
    PartylineChannel *item;

    item = g_object_new (partyline_channel_get_type (), NULL);
    item->number = number;
    item->name = g_strdup (name);
    item->players = g_strdup (players);
    item->state = g_strdup (state);
    item->description = g_strdup (description);

    return item;
}

static void
partyline_label_setup (GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                       GtkListItem *list_item,
                       gpointer data G_GNUC_UNUSED)
{
    GtkWidget *label = gtk_label_new (NULL);

    gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_list_item_set_child (list_item, label);
}

enum {
    PLAYER_COLUMN_NUMBER,
    PLAYER_COLUMN_NAME,
    PLAYER_COLUMN_TEAM
};

static void
partyline_player_bind (GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                       GtkListItem *list_item,
                       gpointer data)
{
    PartylinePlayer *item = gtk_list_item_get_item (list_item);
    GtkWidget *label = gtk_list_item_get_child (list_item);
    const char *text = "";

    switch (GPOINTER_TO_INT (data)) {
    case PLAYER_COLUMN_NUMBER: text = item->number; break;
    case PLAYER_COLUMN_NAME:   text = item->name;   break;
    case PLAYER_COLUMN_TEAM:   text = item->team;   break;
    }

    gtk_label_set_text (GTK_LABEL (label), text != NULL ? text : "");
}

enum {
    CHANNEL_COLUMN_NAME,
    CHANNEL_COLUMN_PLAYERS,
    CHANNEL_COLUMN_STATE,
    CHANNEL_COLUMN_DESCRIPTION
};

static void
partyline_channel_bind (GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                        GtkListItem *list_item,
                        gpointer data)
{
    PartylineChannel *item = gtk_list_item_get_item (list_item);
    GtkWidget *label = gtk_list_item_get_child (list_item);
    const char *text = "";

    switch (GPOINTER_TO_INT (data)) {
    case CHANNEL_COLUMN_NAME:        text = item->name;        break;
    case CHANNEL_COLUMN_PLAYERS:     text = item->players;     break;
    case CHANNEL_COLUMN_STATE:       text = item->state;       break;
    case CHANNEL_COLUMN_DESCRIPTION: text = item->description; break;
    }

    gtk_label_set_text (GTK_LABEL (label), text != NULL ? text : "");
}

static void
partyline_append_column (GtkColumnView *view,
                         const char *title,
                         GCallback bind_callback,
                         int column_id,
                         gboolean expand)
{
    GtkListItemFactory *factory;
    GtkColumnViewColumn *column;

    factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup",
                      G_CALLBACK (partyline_label_setup), NULL);
    g_signal_connect (factory, "bind",
                      bind_callback, GINT_TO_POINTER (column_id));

    column = gtk_column_view_column_new (title, factory);
    gtk_column_view_column_set_resizable (column, TRUE);
    gtk_column_view_column_set_expand (column, expand);
    gtk_column_view_append_column (view, column);

    /* append_column() keeps its own reference. */
    g_object_unref (column);
}

GtkWidget *partyline_page_new (void)
{
    GtkWidget *partyline;
    GtkWidget *left_box;
    GtkWidget *right_box;
    GtkWidget *vertical_paned;
    GtkWidget *chat_box;
    GtkWidget *scroll;
    GtkWidget *frame;
    GtkWidget *info_box;
    GtkWidget *label;
    GtkEventController *key_controller;
    GtkSelectionModel *player_selection;

    work_model = g_list_store_new (partyline_channel_get_type ());
    channellist_model = g_list_store_new (partyline_channel_get_type ());
    playerlist_model = g_list_store_new (partyline_player_get_type ());

    /* Outer horizontal split: chat on the left, player list on the right. */
    partyline = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_focusable (partyline, TRUE);
    gtk_widget_set_margin_start (partyline, 2);
    gtk_widget_set_margin_end (partyline, 2);
    gtk_paned_set_shrink_start_child (GTK_PANED(partyline), FALSE);
    gtk_paned_set_shrink_end_child (GTK_PANED(partyline), FALSE);
    gtk_paned_set_resize_start_child (GTK_PANED(partyline), TRUE);
    gtk_paned_set_resize_end_child (GTK_PANED(partyline), FALSE);

    left_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start (left_box, 2);
    gtk_widget_set_margin_end (left_box, 2);
    gtk_widget_set_margin_top (left_box, 2);
    gtk_widget_set_margin_bottom (left_box, 2);
    gtk_widget_set_hexpand (left_box, TRUE);
    gtk_paned_set_start_child (GTK_PANED(partyline), left_box);

    /*
     * Channel list above the actual partyline text.  This split is vertical
     * and keeps the channel list at its natural/minimum size.
     */
    vertical_paned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_focusable (vertical_paned, TRUE);
    gtk_widget_set_hexpand (vertical_paned, TRUE);
    gtk_widget_set_vexpand (vertical_paned, TRUE);
    gtk_paned_set_resize_start_child (GTK_PANED(vertical_paned), FALSE);
    gtk_paned_set_shrink_start_child (GTK_PANED(vertical_paned), FALSE);
    gtk_paned_set_resize_end_child (GTK_PANED(vertical_paned), TRUE);
    gtk_paned_set_shrink_end_child (GTK_PANED(vertical_paned), TRUE);
    gtk_box_append (GTK_BOX(left_box), vertical_paned);

    channel_list = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL(label), _("<b>Channel List</b>"));
    gtk_box_append (GTK_BOX(channel_list), label);

    channel_selection = gtk_single_selection_new (
        G_LIST_MODEL (g_object_ref (channellist_model)));
    channel_box = gtk_column_view_new (GTK_SELECTION_MODEL (channel_selection));
    gtk_widget_set_focusable (channel_box, TRUE);
    gtk_column_view_set_single_click_activate (GTK_COLUMN_VIEW (channel_box), FALSE);

    partyline_append_column (GTK_COLUMN_VIEW (channel_box), _("Name"),
                             G_CALLBACK (partyline_channel_bind),
                             CHANNEL_COLUMN_NAME, FALSE);
    partyline_append_column (GTK_COLUMN_VIEW (channel_box), _("Players"),
                             G_CALLBACK (partyline_channel_bind),
                             CHANNEL_COLUMN_PLAYERS, FALSE);
    partyline_append_column (GTK_COLUMN_VIEW (channel_box), _("State"),
                             G_CALLBACK (partyline_channel_bind),
                             CHANNEL_COLUMN_STATE, FALSE);
    partyline_append_column (GTK_COLUMN_VIEW (channel_box), _("Description"),
                             G_CALLBACK (partyline_channel_bind),
                             CHANNEL_COLUMN_DESCRIPTION, TRUE);

    g_signal_connect (channel_box, "activate",
                      G_CALLBACK (channel_activated), NULL);

    scroll = gtk_scrolled_window_new ();
    gtk_widget_set_focusable (scroll, TRUE);
    gtk_widget_set_vexpand (scroll, TRUE);
    gtk_widget_set_size_request (scroll, -1, 100);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW(scroll), channel_box);
    gtk_box_append (GTK_BOX(channel_list), scroll);

    gtk_paned_set_start_child (GTK_PANED(vertical_paned), channel_list);

    chat_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand (chat_box, TRUE);

    textboxlabel = gtk_label_new ("");
    gtk_box_append (GTK_BOX(chat_box), textboxlabel);

    textbox = gtk_text_view_new ();
    gtk_widget_set_focusable (textbox, TRUE);
    gtk_text_view_set_editable (GTK_TEXT_VIEW(textbox), FALSE);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW(textbox), GTK_WRAP_WORD);
    gtk_text_view_set_buffer (GTK_TEXT_VIEW(textbox),
                              gtk_text_buffer_new(tag_table));

    scroll = gtk_scrolled_window_new ();
    gtk_widget_set_focusable (scroll, TRUE);
    gtk_widget_set_vexpand (scroll, TRUE);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW(scroll), textbox);
    gtk_box_append (GTK_BOX(chat_box), scroll);

    gtk_paned_set_end_child (GTK_PANED(vertical_paned), chat_box);

    entrybox = gtk_entry_new ();
    gtk_widget_set_focusable (entrybox, TRUE);
    gtk_entry_set_max_length (GTK_ENTRY(entrybox), 200);
    g_signal_connect (entrybox, "activate",
                      G_CALLBACK(textentry), NULL);

    key_controller = gtk_event_controller_key_new ();
    g_signal_connect (key_controller, "key-pressed",
                      G_CALLBACK(entrykey), entrybox);
    gtk_widget_add_controller (entrybox, key_controller);

    gtk_box_append (GTK_BOX(left_box), entrybox);

    /* Right side: players, followed by local player information. */
    right_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start (right_box, 2);
    gtk_widget_set_margin_end (right_box, 2);
    gtk_widget_set_margin_top (right_box, 2);
    gtk_widget_set_margin_bottom (right_box, 2);
    gtk_paned_set_end_child (GTK_PANED(partyline), right_box);

    player_selection = GTK_SELECTION_MODEL (
        gtk_no_selection_new (G_LIST_MODEL (g_object_ref (playerlist_model))));
    playerlist = gtk_column_view_new (player_selection);
    gtk_widget_set_focusable (playerlist, TRUE);

    partyline_append_column (GTK_COLUMN_VIEW (playerlist), "",
                             G_CALLBACK (partyline_player_bind),
                             PLAYER_COLUMN_NUMBER, FALSE);
    partyline_append_column (GTK_COLUMN_VIEW (playerlist), _("Name"),
                             G_CALLBACK (partyline_player_bind),
                             PLAYER_COLUMN_NAME, TRUE);
    partyline_append_column (GTK_COLUMN_VIEW (playerlist), _("Team"),
                             G_CALLBACK (partyline_player_bind),
                             PLAYER_COLUMN_TEAM, TRUE);

    scroll = gtk_scrolled_window_new ();
    gtk_widget_set_focusable (scroll, TRUE);
    gtk_widget_set_vexpand (scroll, TRUE);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW(scroll), playerlist);
    gtk_box_append (GTK_BOX(right_box), scroll);

    frame = gtk_frame_new (NULL);
    info_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_frame_set_child (GTK_FRAME(frame), info_box);

    label = gtk_label_new (_("Your name:"));
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_box_append (GTK_BOX(info_box), label);

    namelabel = gtk_label_new ("");
    gtk_label_set_justify (GTK_LABEL(namelabel), GTK_JUSTIFY_CENTER);
    gtk_label_set_wrap (GTK_LABEL(namelabel), TRUE);
    gtk_label_set_wrap_mode (GTK_LABEL(namelabel), PANGO_WRAP_WORD_CHAR);
    gtk_box_append (GTK_BOX(info_box), namelabel);

    label = gtk_label_new (_("Your team:"));
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_box_append (GTK_BOX(info_box), label);

    teamlabel = gtk_label_new ("");
    gtk_label_set_justify (GTK_LABEL(teamlabel), GTK_JUSTIFY_CENTER);
    gtk_label_set_wrap (GTK_LABEL(teamlabel), TRUE);
    gtk_label_set_wrap_mode (GTK_LABEL(teamlabel), PANGO_WRAP_WORD_CHAR);
    gtk_box_append (GTK_BOX(info_box), teamlabel);

    gtk_box_append (GTK_BOX(info_box),
                    gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

    infolabel = gtk_label_new ("");
    gtk_widget_set_halign (infolabel, GTK_ALIGN_END);
    gtk_label_set_justify (GTK_LABEL(infolabel), GTK_JUSTIFY_RIGHT);
    gtk_label_set_wrap (GTK_LABEL(infolabel), TRUE);
    gtk_box_append (GTK_BOX(info_box), infolabel);

    gtk_box_append (GTK_BOX(right_box), frame);

    /* Set a few things. */
    partyline_connectstatus (FALSE);
    plhistory[0][0] = 0;
    gtk_paned_set_position (GTK_PANED(partyline), 550);

    return partyline;
}

void partyline_connectstatus (int status)
{
    if (status) {
        gtk_widget_set_sensitive (entrybox, TRUE);
    }
    else {
        gtk_widget_set_sensitive (entrybox, FALSE);
    }
}

void partyline_namelabel (char *nick, char *team)
{
    if (nick)
    {
      gtk_label_set_text (GTK_LABEL(namelabel), nick);
    }
    else gtk_label_set_text (GTK_LABEL(namelabel), "");
    if (team)
    {
      gtk_label_set_text (GTK_LABEL(teamlabel), team);
    }
    else gtk_label_set_text (GTK_LABEL(teamlabel), "");
}

void partyline_status (char *status)
{
    gtk_label_set_text (GTK_LABEL(infolabel), status);
}

void partyline_text (const gchar *text)
{
    if (timestampsenable) {
        time_t now;
        char buf[1024];
        char timestamp[9];

        now = time(NULL);

        strftime(timestamp, sizeof(timestamp), "%H:%M:%S", localtime(&now));
        g_snprintf (buf, sizeof(buf), "%c[%s]%c %s",
                    TETRI_TB_C_GREY, timestamp, TETRI_TB_RESET, text);

        textbox_addtext (GTK_TEXT_VIEW(textbox), buf);
    }
    else
        textbox_addtext (GTK_TEXT_VIEW(textbox), text);

    adjust_bottom_text_view(GTK_TEXT_VIEW(textbox));
}

void partyline_fmt (const char *fmt, ...)
{
  va_list ap;
  char *text = NULL;

  va_start(ap, fmt);
  text = g_strdup_vprintf(fmt, ap);
  va_end(ap);

  partyline_text(text); g_free(text);
}

void partyline_playerlist (int *numbers, char **names, char **teams, int n, char **specs, int sn)
{
    int i;
    char buf0[16], buf1[128], buf2[128];

    g_list_store_remove_all (playerlist_model);

    for (i = 0; i < n; i++) {
        PartylinePlayer *item;

        g_snprintf (buf0, sizeof(buf0), "%d", numbers[i]);
        GTET_O_STRCPY (buf1, nocolor(names[i]));
        GTET_O_STRCPY (buf2, nocolor(teams[i]));

        item = partyline_player_new (buf0, buf1, buf2);
        g_list_store_append (playerlist_model, item);
        g_object_unref (item);
    }

    {
        PartylinePlayer *separator = partyline_player_new ("", "", "");
        g_list_store_append (playerlist_model, separator);
        g_object_unref (separator);
    }

    for (i = 0; i < sn; i++) {
        PartylinePlayer *item;

        GTET_O_STRCPY (buf1, nocolor(specs[i]));
        item = partyline_player_new ("S", buf1, "");
        g_list_store_append (playerlist_model, item);
        g_object_unref (item);
    }
}

void partyline_entryfocus (void)
{
    if (connected)
    {
      gtk_editable_set_text (GTK_EDITABLE (entrybox), "");
      gtk_editable_set_position (GTK_EDITABLE (entrybox), 0);
      gtk_widget_grab_focus (entrybox);
    }
}

void textentry (GtkWidget *widget)
{
    const char *text;
    text = gtk_editable_get_text (GTK_EDITABLE(widget));

    if (strlen(text) == 0) return;

    if (g_str_has_prefix(text, "/list"))
      stop_list(); /* Parsing can't be perfect,
                      so make sure they can do it by hand... */
    
    // Show the command if it's a /msg
    if (g_str_has_prefix (text, "/msg"))
      partyline_text (text);
    
    tetrinet_playerline (text);
    GTET_O_STRCPY (plhistory[plh_end], text);
    gtk_editable_set_text (GTK_EDITABLE(widget), "");

    plh_end ++;
    if (plh_end == PLHSIZE) plh_end = 0;
    if (plh_end == plh_start) plh_start ++;
    if (plh_start == PLHSIZE) plh_start = 0;
    plh_cur = plh_end;

}

static void playerlist_complete_nick (void)
{
    gchar *text;
    guint i, count;

    text = g_utf8_strdown (gtk_editable_get_text (GTK_EDITABLE (entrybox)), -1);
    if (text == NULL)
        return;

    count = g_list_model_get_n_items (G_LIST_MODEL (playerlist_model));

    for (i = 0; i < count; i++) {
        PartylinePlayer *item;
        gchar *down;

        item = g_list_model_get_item (G_LIST_MODEL (playerlist_model), i);
        if (item == NULL)
            continue;

        down = g_utf8_strdown (item->name != NULL ? item->name : "", -1);
        if (g_str_has_prefix (down, text)) {
            gchar *aux = g_strconcat (item->name, ": ", NULL);

            gtk_editable_set_text (GTK_EDITABLE (entrybox), aux);
            gtk_editable_set_position (GTK_EDITABLE (entrybox), -1);

            g_free (aux);
            g_free (down);
            g_object_unref (item);
            break;
        }

        g_free (down);
        g_object_unref (item);
    }

    g_free (text);
}

static gboolean entrykey (GtkEventControllerKey *controller,
                           guint keyval,
                           guint keycode,
                           GdkModifierType state,
                           gpointer user_data)
{
    GtkWidget *widget = GTK_WIDGET (user_data);
    gchar *text = NULL;

    (void)controller;
    (void)keycode;
    (void)state;

    if (keyval == GDK_KEY_Up || keyval == GDK_KEY_Down) {
        if (plh_cur == plh_end) {
            GTET_O_STRCPY (plhistory[plh_end],
                           gtk_editable_get_text (GTK_EDITABLE(widget)));
        }

        switch (keyval) {
        case GDK_KEY_Up:
            if (plh_cur == plh_start) break;
            plh_cur --;
            if (plh_cur == -1) plh_cur = PLHSIZE - 1;
            break;
        case GDK_KEY_Down:
            if (plh_cur == plh_end) break;
            plh_cur ++;
            if (plh_cur == PLHSIZE) plh_cur = 0;
            break;
        }

        text = plhistory[plh_cur];
        gtk_editable_set_text (GTK_EDITABLE(widget), text);
        gtk_editable_set_position (GTK_EDITABLE (widget), -1);
#ifdef DEBUG
        printf ("history: %d %d %d %s", plh_start, plh_end, plh_cur, text);
#endif
        return TRUE;
    }
    else if (keyval == GDK_KEY_Left || keyval == GDK_KEY_Right) {
        return FALSE;
    }
    else if (keyval == GDK_KEY_Tab) {
        playerlist_complete_nick ();
        return TRUE;
    }
    else {
        plh_cur = plh_end;
        return FALSE;
    }
}

void partyline_add_channel (gchar *line)
{
  GScanner *scan;
  gint num, actual, max;
  gchar *name, *players, *state, final[1024], *desc, *utf8;
  scan = g_scanner_new (NULL);
  g_scanner_input_text (scan, line, strlen (line));
  
  scan->config->cpair_comment_single = ""; // in jetrix, channels don't start with a '#' in list; use [ to start another token after channel name (tetrinet-server does not leave a space before [)
  scan->config->skip_comment_single = FALSE;
  scan->config->cset_skip_characters = " \n\t[";
  scan->config->scan_identifier_1char = TRUE;
  
/*
  while ((g_scanner_get_next_token (scan) != G_TOKEN_LEFT_PAREN) && !g_scanner_eof (scan));
  g_scanner_get_next_token (scan); // dump the '('
  num = g_ascii_strtoull(scan->value.v_string, NULL, 10); // the number is now a string entity, so we convert it ourself (v_int is badly converted)
*/
  while ((g_scanner_get_next_token (scan) != G_TOKEN_INT) && !g_scanner_eof (scan));
  num = (scan->token==G_TOKEN_INT) ? scan->value.v_int : 0; 

  g_scanner_get_next_token (scan); /* dump the ')' */
  scan->config->cset_identifier_first = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; // tokens can start with any character, but as identifiers take precedence, we can't detect INT anymore
  
  if (g_scanner_peek_next_token (scan) == G_TOKEN_LEFT_BRACE)
  {
    scan->config->cpair_comment_single = "# ";
    
    while ((g_scanner_get_next_token (scan) != G_TOKEN_INT) && !g_scanner_eof (scan));
    actual = (scan->token==G_TOKEN_INT) ? scan->value.v_int : 0;
    
    while ((g_scanner_get_next_token (scan) != G_TOKEN_INT) && !g_scanner_eof (scan));
    max = (scan->token==G_TOKEN_INT) ? scan->value.v_int : 0;

    while ((g_scanner_get_next_token (scan) != G_TOKEN_COMMENT_SINGLE) && !g_scanner_eof (scan));
    /* This will be utf-8 since it's converted in client_readmsg, but just in
     * case the parsing code splits up a char sequence.. - vidar
     */
    utf8 = ensure_utf8((scan->token==G_TOKEN_COMMENT_SINGLE) ? scan->value.v_comment : "");
    name = g_strconcat ("#", utf8, NULL);
    
    g_snprintf (final, 1024, "%d/%d", actual, max);

    scan->config->cpair_comment_single = "{}";
    while ((g_scanner_get_next_token (scan) != G_TOKEN_COMMENT_SINGLE) && !g_scanner_eof (scan));
    if (!g_scanner_eof (scan))
      state = g_strdup ((scan->token==G_TOKEN_COMMENT_SINGLE) ? scan->value.v_comment : "");
    else
      state = g_strdup ("IDLE");

    desc = g_strdup ("");
    players = g_strdup ("");
  }
  else // tetrinet-server & jetrix
  {
    while ((g_scanner_get_next_token (scan) != G_TOKEN_IDENTIFIER) && !g_scanner_eof (scan)); // in jetrix, channels don't start with a '#' in list, this supports channels starting with and without '#'
    utf8 = ensure_utf8 ((scan->token==G_TOKEN_IDENTIFIER) ? scan->value.v_identifier : "");
    name = g_strconcat ("#", utf8, NULL);

    while ((g_scanner_get_next_token (scan) != G_TOKEN_IDENTIFIER) && !g_scanner_eof (scan));
    players = g_strdup ((scan->token==G_TOKEN_IDENTIFIER) ? scan->value.v_identifier : "");

    if (players != NULL)
    {
      if (strncmp (players, "FULL", 4))
      {
        scan->config->cset_identifier_first = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        while ((g_scanner_get_next_token (scan) != G_TOKEN_INT) && !g_scanner_eof (scan));
        actual = (scan->token==G_TOKEN_INT) ? scan->value.v_int : 0;

        while ((g_scanner_get_next_token (scan) != G_TOKEN_INT) && !g_scanner_eof (scan));
        max = (scan->token==G_TOKEN_INT) ? scan->value.v_int : 0;

        g_snprintf (final, 1024, "%d/%d %s", actual, max, players);
        scan->config->cset_identifier_first = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
      }
      else
        g_snprintf (final, 1024, "%s", players);
    }
    else
      g_snprintf (final, 1024, "UNK");

    g_scanner_get_next_token (scan); /* dump the ']' */

    scan->config->cpair_comment_single = "{}";
    while ((g_scanner_get_next_token (scan) != G_TOKEN_COMMENT_SINGLE) && !g_scanner_eof (scan));
    if (!g_scanner_eof (scan))
      state = g_strdup ((scan->token==G_TOKEN_COMMENT_SINGLE) ? scan->value.v_comment : ""); // INGAME
    else
      state = g_strdup ("IDLE");
  
    // give us the rest of the line
    if (!g_scanner_eof(scan) && (scan->position < strlen(line)))
      desc = g_strstrip (ensure_utf8 (&line[scan->position]));
    else
      desc = g_strdup ("");
  }
  
  
  {
    PartylineChannel *item;

    item = partyline_channel_new (num, name, final, state, desc);
    g_list_store_append (work_model, item);
    g_object_unref (item);
  }

  g_scanner_destroy (scan);
  g_free (name);
  g_free (state);
  g_free (players);
  g_free (desc);
  g_free (utf8);
}

void stop_list (void)
{
    guint i, count;

    list_issued = 0;

    /*
     * Keep the old double-buffering behaviour: /list replies accumulate in
     * work_model and become visible only when the batch is complete.
     */
    g_list_store_remove_all (channellist_model);

    count = g_list_model_get_n_items (G_LIST_MODEL (work_model));
    for (i = 0; i < count; i++) {
        PartylineChannel *item;

        item = g_list_model_get_item (G_LIST_MODEL (work_model), i);
        if (item == NULL)
            continue;

        g_list_store_append (channellist_model, item);
        g_object_unref (item);
    }
}

gboolean partyline_update_channel_list (void)
{
  gchar cad[1024];
  
  /* if there is another update in progress, just go away silently */
  if (connected && list_enabled && (list_issued == 0))
  {
    list_issued++;
    g_list_store_remove_all (work_model);
    tetrinet_playerline ("/list");
  
    /* send the mark */
    g_snprintf (cad, 1024, "/msg %d --- MARK ---", playernum);
    tetrinet_playerline (cad);
  }
  
  return TRUE;
}

void partyline_more_channel_lines (void)
{
  gchar cad[1024];

  list_issued ++;
  tetrinet_playerline ("/list+");
  g_snprintf (cad, 1024, "/msg %d --- MARK ---", playernum);
  tetrinet_playerline (cad);
}

void partyline_clear_list_channel (void)
{
  g_list_store_remove_all (channellist_model);
  g_list_store_remove_all (work_model);
}

static void channel_activated (GtkColumnView *view G_GNUC_UNUSED,
                               guint position,
                               gpointer data G_GNUC_UNUSED)
{
    PartylineChannel *item;
    gchar *cad;

    item = g_list_model_get_item (G_LIST_MODEL (channellist_model), position);
    if (item == NULL)
        return;

    cad = g_strconcat ("/join ", item->name, NULL);
    tetrinet_playerline (cad);

    g_free (cad);
    g_object_unref (item);
}

void partyline_joining_channel (const gchar *channel)
{
  gchar *final;
  
  if (channel != NULL)
    final = g_strconcat ("<b>", _("Talking in channel"), " ", channel, "</b>", NULL);
  else
    final = g_strconcat ("<b>", _("Disconnected"), "</b>", NULL);

  gtk_label_set_markup (GTK_LABEL (textboxlabel), final);
  
  g_free (final);
}

void partyline_show_channel_list (gboolean show)
{
  /*
   * If this function is called with TRUE, it will show the channel list, otherwise
   * it'll hide it.
   * If there is no channel_list yet, do nothing
   */
  if(channel_list)
  {
    list_enabled = show;
    if (list_enabled)
    {
      gtk_widget_set_visible (channel_list, TRUE);
      partyline_update_channel_list ();
    }
    else
      gtk_widget_set_visible (channel_list, FALSE);
  }
}
