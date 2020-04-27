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
    *namelabel, *teamlabel, *infolabel, *textboxscroll, 
    *playerlist_scroll, *playerlist_vpaned, *channel_box,
    *playerlist_channel_scroll, *label, *channel_list;

/* some more widgets for layout */
static GtkWidget *table, *leftbox, *rightbox;

static GtkListStore *work_model, *playerlist_channels;

/* stuff for pline history */
#define PLHSIZE 64
char plhistory[PLHSIZE][256];
int plh_start = 0, plh_end = 0, plh_cur = 0;

/* function prototypes for callbacks */
static void textentry (GtkWidget *widget);
static gint entrykey (GtkWidget *widget, GdkEventKey *key);
void channel_activated (GtkTreeView *treeview);

GtkWidget *partyline_page_new (void)
{
    GtkWidget *widget, *box, *box2; /* generic temp variables */
    GtkListStore *playerlist_model = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
    GList *focus_list = NULL;
    gchar* aux;
    GtkBuilder *builder;

    work_model = gtk_list_store_new (5, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  
    /* left box */
    leftbox = gtk_vbox_new (FALSE, 4);
    /* chat thingy */
    /* channel list */
    channel_list = gtk_vbox_new (FALSE, 0);
    builder = gtk_builder_new_from_resource("/org/gtetrinet/channel_list.ui");
    // info about attributes: see gtk_tree_view_column_add_attribute
    playerlist_channels = gtk_builder_get_object(builder, "channellist_model");
    channel_box = gtk_builder_get_object(builder, "channel_list");

    g_signal_connect (G_OBJECT (channel_box), "row-activated",
                      G_CALLBACK (channel_activated), NULL);
    playerlist_channel_scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (playerlist_channel_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER(playerlist_channel_scroll), channel_box);
    gtk_widget_set_size_request (playerlist_channel_scroll, -1, 100);
    label = gtk_label_new (NULL);

    aux = g_strconcat ("<b>", _("Channel List"), "</b>", NULL);
    gtk_label_set_markup (GTK_LABEL (label), aux);
    g_free (aux);
    gtk_box_pack_start (GTK_BOX (channel_list), label, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (channel_list), playerlist_channel_scroll, TRUE, TRUE, 0);

    /* textbox with scrollbars */
    box2 = gtk_vbox_new (FALSE, 0);
    textbox = gtk_text_view_new_with_buffer(gtk_text_buffer_new(tag_table));
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW(textbox), GTK_WRAP_WORD);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (textbox), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textbox), FALSE);
    textboxscroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(textboxscroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_ALWAYS);
    gtk_container_add (GTK_CONTAINER(textboxscroll), textbox);
    label = gtk_label_new (NULL);
    partyline_joining_channel (NULL);
    gtk_box_pack_start (GTK_BOX (box2), label, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box2), textboxscroll, TRUE, TRUE, 0);
    
    /* vpaned widget */
    playerlist_vpaned = gtk_vpaned_new ();
    
    gtk_paned_add1 (GTK_PANED (playerlist_vpaned), channel_list);
    gtk_paned_add2 (GTK_PANED (playerlist_vpaned), box2);
    gtk_box_pack_start (GTK_BOX(leftbox), playerlist_vpaned, TRUE, TRUE, 0);
    
    /* entry box */
    entrybox = gtk_entry_new ();
    gtk_entry_set_max_length (GTK_ENTRY (entrybox), 200);
    g_signal_connect (G_OBJECT(entrybox), "activate",
                      G_CALLBACK(textentry), NULL);
    g_signal_connect (G_OBJECT(entrybox), "key-press-event",
                      G_CALLBACK(entrykey), NULL);
    gtk_box_pack_start (GTK_BOX(leftbox), entrybox, FALSE, FALSE, 0);
    gtk_widget_show_all (leftbox);

    /* player list with scrollbar */
    playerlist = GTK_WIDGET (gtk_tree_view_new_with_model (GTK_TREE_MODEL (playerlist_model)));
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (playerlist), -1, "", renderer,
                                                 "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (playerlist), -1, _("Name"), renderer,
                                                 "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (playerlist), -1, _("Team"), renderer,
                                                 "text", 2, NULL);
    playerlist_scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (playerlist_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_NEVER);
    gtk_container_add (GTK_CONTAINER(playerlist_scroll), playerlist);
    gtk_widget_set_size_request (playerlist_scroll, 150, 200);
    gtk_widget_show_all (playerlist_scroll);
    
    /* right box */
    box = gtk_vbox_new (FALSE, 2);

    widget = leftlabel_new (_("Your name:"));
    gtk_box_pack_start (GTK_BOX(box), widget, FALSE, FALSE, 0);
    namelabel = gtk_label_new ("");
    gtk_widget_show (namelabel);
    gtk_box_pack_start (GTK_BOX(box), namelabel, FALSE, FALSE, 0);
    widget = gtk_vseparator_new (); /* invisible... just needed some space */
    gtk_box_pack_start (GTK_BOX(box), widget, FALSE, FALSE, 2);
    widget = leftlabel_new (_("Your team:"));
    gtk_box_pack_start (GTK_BOX(box), widget, FALSE, FALSE, 0);
    teamlabel = gtk_label_new ("");
    gtk_box_pack_start (GTK_BOX(box), teamlabel, FALSE, FALSE, 0);
    infolabel = gtk_label_new ("");
    gtk_label_set_line_wrap (GTK_LABEL(teamlabel), TRUE);
    gtk_box_pack_start (GTK_BOX(box), infolabel, TRUE, FALSE, 0);

    gtk_container_set_border_width (GTK_CONTAINER(box), 4);
    rightbox = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(rightbox), GTK_SHADOW_IN);
    gtk_container_add (GTK_CONTAINER(rightbox), box);
    gtk_widget_show_all (rightbox);

    /* stuff all the boxes into the table */
    table = gtk_table_new (2, 2, FALSE);
    gtk_table_set_row_spacings (GTK_TABLE(table), 4);
    gtk_table_set_col_spacings (GTK_TABLE(table), 4);
    gtk_container_set_border_width (GTK_CONTAINER(table), 4);
    gtk_table_attach (GTK_TABLE(table), leftbox, 0, 1, 0, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
                      0, 0);
    gtk_table_attach (GTK_TABLE(table), playerlist_scroll, 1, 2, 0, 1,
                      GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
    gtk_table_attach (GTK_TABLE(table), rightbox, 1, 2, 1, 2,
                      GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);

    /* set a few things */
    partyline_connectstatus (FALSE);
    plhistory[0][0] = 0;
    
    focus_list = g_list_append (focus_list, entrybox);
    focus_list = g_list_append (focus_list, textbox);
    focus_list = g_list_append (focus_list, playerlist);
    
    gtk_container_set_focus_chain (GTK_CONTAINER (table), focus_list);

    /* show after we return */
    return table;
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
    GtkListStore *playerlist_model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (playerlist)));
    GtkTreeIter iter;

    /* update the playerlist so that it contains only the given names */
    gtk_list_store_clear (playerlist_model);

    for (i = 0; i < n; i ++) {
        g_snprintf (buf0, sizeof(buf0), "%d", numbers[i]);
        GTET_O_STRCPY (buf1, nocolor(names[i]));
        GTET_O_STRCPY (buf2, nocolor(teams[i]));
   
        gtk_list_store_append (playerlist_model, &iter);
        gtk_list_store_set (playerlist_model, &iter,
                            0, buf0, 1, buf1, 2, buf2, -1);
    }

    buf0[0] = buf1[0] = buf2[0] = 0;
    gtk_list_store_append (playerlist_model, &iter);
    gtk_list_store_set (playerlist_model, &iter,
                        0, buf0, 1, buf1, 2, buf2, -1);

    for (i = 0; i < sn; i ++) {
        GTET_O_STRCPY (buf0, "S");
        GTET_O_STRCPY (buf1, nocolor(specs[i]));
        GTET_O_STRCPY (buf2, "");
        gtk_list_store_append (playerlist_model, &iter);
        gtk_list_store_set (playerlist_model, &iter,
                            0, buf0, 1, buf1, 2, buf2, -1);
    }
}

void partyline_entryfocus (void)
{
    if (connected)
    {
      gtk_entry_set_text (GTK_ENTRY (entrybox), "");
      gtk_editable_set_position (GTK_EDITABLE (entrybox), 0);
      gtk_widget_grab_focus (entrybox);
    }
}

void textentry (GtkWidget *widget)
{
    const char *text;
    text = gtk_entry_get_text (GTK_ENTRY(widget));

    if (strlen(text) == 0) return;

    if (g_str_has_prefix(text, "/list"))
      stop_list(); /* Parsing can't be perfect,
                      so make sure they can do it by hand... */
    
    // Show the command if it's a /msg
    if (g_str_has_prefix (text, "/msg"))
      partyline_text (text);
    
    tetrinet_playerline (text);
    GTET_O_STRCPY (plhistory[plh_end], text);
    gtk_entry_set_text (GTK_ENTRY(widget), "");

    plh_end ++;
    if (plh_end == PLHSIZE) plh_end = 0;
    if (plh_end == plh_start) plh_start ++;
    if (plh_start == PLHSIZE) plh_start = 0;
    plh_cur = plh_end;

}

static gboolean is_nick (GtkTreeModel *model,
                         GtkTreePath *path,
                         GtkTreeIter *iter,
                         gpointer data)
{
  gchar *nick, *aux, *down;

  gtk_tree_model_get (model, iter, 1, &nick, -1);
  down = g_utf8_strdown (nick, -1);
  path = path;

  if (g_str_has_prefix (down, data))
  {
    aux = g_strconcat (nick, ": ", NULL);
    gtk_entry_set_text (GTK_ENTRY (entrybox), aux);
    gtk_editable_set_position (GTK_EDITABLE (entrybox), -1);

    g_free (aux);
    g_free (nick);
    g_free (down);
    return TRUE;
  }
  else
  {
    g_free (nick);
    g_free (down);
    return FALSE;
  }
}

static void playerlist_complete_nick (void)
{
  GtkListStore *playerlist_model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (playerlist)));
  gchar *text;

  text = g_utf8_strdown (gtk_entry_get_text (GTK_ENTRY (entrybox)), -1);
  if (text == NULL) return;

  gtk_tree_model_foreach (GTK_TREE_MODEL (playerlist_model), is_nick, text);

  g_free (text);
}

static gint entrykey (GtkWidget *widget, GdkEventKey *key)
{
    int keyval = key->keyval;
    gchar *text = NULL;

    if (keyval == GDK_KEY_Up || keyval == GDK_KEY_Down) {
        if (plh_cur == plh_end) {
            GTET_O_STRCPY (plhistory[plh_end], gtk_entry_get_text(GTK_ENTRY(widget)));
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
        gtk_entry_set_text (GTK_ENTRY(widget), text);
        gtk_editable_set_position (GTK_EDITABLE (widget), -1);
#ifdef DEBUG
        printf ("history: %d %d %d %s\n", plh_start, plh_end, plh_cur,
                plhistory[plh_cur]);
#endif
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;
    }
    else if (keyval == GDK_KEY_Left || keyval == GDK_KEY_Right) {
        return FALSE;
    }
    else if (keyval == GDK_KEY_Tab)
    {
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
  GtkTreeIter iter;
  
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
  
  
  gtk_list_store_append (work_model, &iter);
  gtk_list_store_set (work_model, &iter,
                      0, num,
                      1, name,
                      2, final,
                      3, state,
                      4, desc,
                      -1);

  g_scanner_destroy (scan);
  g_free (name);
  g_free (state);
  g_free (players);
  g_free (desc);
  g_free (utf8);
}

gboolean copy_item (GtkTreeModel *model,
                    GtkTreePath *path,
                    GtkTreeIter *iter)
{
  gint num;
  gchar *name, *players, *state, *desc;
  GtkTreeIter iter2;

  path = path;
  
  gtk_tree_model_get (model, iter,
                      0, &num,
                      1, &name,
                      2, &players,
                      3, &state,
                      4, &desc, -1);
  
  gtk_list_store_append (playerlist_channels, &iter2);
  gtk_list_store_set (playerlist_channels, &iter2,
                      0, num,
                      1, name,
                      2, players,
                      3, state,
                      4, desc,
                      -1);
  
  g_free (players);
  g_free (name);
  g_free (state);
  g_free (desc);
  
  return FALSE;
}

void stop_list (void)
{
  list_issued = 0;
  
  /* update the channel list widget, with some sort of "double buffering" */
  gtk_tree_view_set_model (GTK_TREE_VIEW (channel_box), GTK_TREE_MODEL (work_model));
  gtk_list_store_clear (playerlist_channels);
  gtk_tree_model_foreach (GTK_TREE_MODEL (work_model), (GtkTreeModelForeachFunc) copy_item, NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (channel_box), GTK_TREE_MODEL (playerlist_channels));
}

gboolean partyline_update_channel_list (void)
{
  gchar cad[1024];
  
  /* if there is another update in progress, just go away silently */
  if (connected && list_enabled && (list_issued == 0))
  {
    list_issued++;
    gtk_list_store_clear (work_model);
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
  gtk_list_store_clear (playerlist_channels);
  gtk_list_store_clear (work_model);
}

void channel_activated (GtkTreeView *treeview)
{
  GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
  GtkTreeIter iter;
  gchar *name, *cad;
  
  gtk_tree_selection_get_selected (selection, NULL, &iter);
  gtk_tree_model_get (GTK_TREE_MODEL (playerlist_channels),
                      &iter,
                      1, &name, -1);
  
  cad = g_strconcat ("/join ", name, NULL);
  tetrinet_playerline (cad);
  
  g_free (name);
  g_free (cad);
}

void partyline_joining_channel (const gchar *channel)
{
  gchar *final;
  
  if (channel != NULL)
    final = g_strconcat ("<b>", _("Talking in channel"), " ", channel, "</b>", NULL);
  else
    final = g_strconcat ("<b>", _("Disconnected"), "</b>", NULL);

  gtk_label_set_markup (GTK_LABEL (label), final);
  
  g_free (final);
}

void partyline_show_channel_list (gboolean show)
{
  /*
   * If this function is called with TRUE, it will show the channel list, otherwise
   * it'll hide it.
   */
  list_enabled = show;
  if (list_enabled)
  {
    gtk_widget_show (channel_list);
    partyline_update_channel_list ();
  }
  else
    gtk_widget_hide (channel_list);
}
