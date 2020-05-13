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

static GtkListStore *work_model, *channellist_model;

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
    GtkBuilder *builder;

    work_model = gtk_list_store_new (5, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    builder = gtk_builder_new_from_resource("/apps/gtetrinet/partyline.ui");
    // info about attributes: see gtk_tree_view_column_add_attribute
    channellist_model = GTK_LIST_STORE (gtk_builder_get_object(builder, "channellist_model")); // model to add entries in list
    channel_box = GTK_WIDGET (gtk_builder_get_object(builder, "channellist_treeview")); // tree view to change model, see stop_list function
    channel_list = GTK_WIDGET (gtk_builder_get_object(builder, "channellist"));
    g_signal_connect (G_OBJECT (channel_box), "row-activated",
                      G_CALLBACK (channel_activated), NULL);
    entrybox = GTK_WIDGET (gtk_builder_get_object(builder, "entrybox"));
    g_signal_connect (G_OBJECT(entrybox), "activate",
                      G_CALLBACK(textentry), NULL);
    g_signal_connect (G_OBJECT(entrybox), "key-press-event",
                      G_CALLBACK(entrykey), NULL);
    infolabel = GTK_WIDGET (gtk_builder_get_object(builder, "infolabel"));
    textbox = GTK_WIDGET (gtk_builder_get_object(builder, "textbox"));
    textboxlabel = GTK_WIDGET (gtk_builder_get_object(builder, "textboxlabel"));
    gtk_text_view_set_buffer( GTK_TEXT_VIEW (textbox), gtk_text_buffer_new(tag_table));
    playerlist = GTK_WIDGET (gtk_builder_get_object(builder, "playerlist"));
    namelabel = GTK_WIDGET (gtk_builder_get_object(builder, "namelabel"));
    teamlabel = GTK_WIDGET (gtk_builder_get_object(builder, "teamlabel"));

    /* set a few things */
    partyline_connectstatus (FALSE);
    plhistory[0][0] = 0;

    return GTK_WIDGET (gtk_builder_get_object(builder, "partyline"));
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

  gtk_tree_model_get (model, iter,
                      0, &num,
                      1, &name,
                      2, &players,
                      3, &state,
                      4, &desc, -1);
  
  gtk_list_store_append (channellist_model, &iter2);
  gtk_list_store_set (channellist_model, &iter2,
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
  gtk_list_store_clear (channellist_model);
  gtk_tree_model_foreach (GTK_TREE_MODEL (work_model), (GtkTreeModelForeachFunc) copy_item, NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (channel_box), GTK_TREE_MODEL (channellist_model));
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
  gtk_list_store_clear (channellist_model);
  gtk_list_store_clear (work_model);
}

void channel_activated (GtkTreeView *treeview)
{
  GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
  GtkTreeIter iter;
  gchar *name, *cad;
  
  gtk_tree_selection_get_selected (selection, NULL, &iter);
  gtk_tree_model_get (GTK_TREE_MODEL (channellist_model),
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
      gtk_widget_show (channel_list);
      partyline_update_channel_list ();
    }
    else
      gtk_widget_hide (channel_list);
  }
}
