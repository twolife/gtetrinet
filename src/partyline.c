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
#include <stdarg.h>

#include "client.h"
#include "tetrinet.h"
#include "partyline.h"
#include "misc.h"
#include "commands.h"

/* widgets that we have to do stuff with */
static GtkWidget *playerlist, *textbox, *entrybox,
    *namelabel, *teamlabel, *infolabel, *textboxscroll, *playerlist_scroll;

/* some more widgets for layout */
static GtkWidget *table, *leftbox, *rightbox;

/* stuff for pline history */
#define PLHSIZE 64
char plhistory[PLHSIZE][256];
int plh_start = 0, plh_end = 0, plh_cur = 0;

/* function prototypes for callbacks */
static void textentry (GtkWidget *widget);
static gint entrykey (GtkWidget *widget, GdkEventKey *key);

GtkWidget *partyline_page_new (void)
{
    GtkWidget *widget, *box; /* generic temp variables */
    GtkListStore *playerlist_model = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();

    /* left box */
    leftbox = gtk_vbox_new (FALSE, 4);
    /* chat thingy */
    /* textbox with scrollbars */
    textbox = gtk_text_view_new_with_buffer(gtk_text_buffer_new(tag_table));
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW(textbox), GTK_WRAP_WORD);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (textbox), FALSE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textbox), FALSE);
    gtk_widget_show (textbox);
    textboxscroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(textboxscroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_ALWAYS);
    gtk_container_add (GTK_CONTAINER(textboxscroll), textbox);
    gtk_widget_show(textboxscroll);
    gtk_box_pack_start (GTK_BOX(leftbox), textboxscroll, TRUE, TRUE, 0);
    /* entry box */
    entrybox = gtk_entry_new_with_max_length (200);
    g_signal_connect (G_OBJECT(entrybox), "activate",
                      GTK_SIGNAL_FUNC(textentry), NULL);
    g_signal_connect (G_OBJECT(entrybox), "key-press-event",
                      GTK_SIGNAL_FUNC(entrykey), NULL);
    gtk_widget_show (entrybox);
    gtk_box_pack_start (GTK_BOX(leftbox), entrybox, FALSE, FALSE, 0);
    gtk_widget_show (leftbox);

    /* player list with scrollbar */
    playerlist = GTK_WIDGET (gtk_tree_view_new_with_model (GTK_TREE_MODEL (playerlist_model)));
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (playerlist), -1, "", renderer,
                                                 "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (playerlist), -1, _("Name"), renderer,
                                                 "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (playerlist), -1, _("Team"), renderer,
                                                 "text", 2, NULL);
    gtk_widget_show (playerlist);
    playerlist_scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (playerlist_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_NEVER);
    gtk_container_add (GTK_CONTAINER(playerlist_scroll), playerlist);
    gtk_widget_set_usize (playerlist_scroll, 150, 200);
    gtk_widget_show (playerlist_scroll);
    
    /* right box */
    box = gtk_vbox_new (FALSE, 2);

    widget = leftlabel_new (_("Your name:"));
    gtk_widget_show (widget);
    gtk_box_pack_start (GTK_BOX(box), widget, FALSE, FALSE, 0);
    namelabel = gtk_label_new ("");
    gtk_widget_show (namelabel);
    gtk_box_pack_start (GTK_BOX(box), namelabel, FALSE, FALSE, 0);
    widget = gtk_vseparator_new (); /* invisible... just needed some space */
    gtk_widget_show (widget);
    gtk_box_pack_start (GTK_BOX(box), widget, FALSE, FALSE, 2);
    widget = leftlabel_new (_("Your team:"));
    gtk_widget_show (widget);
    gtk_box_pack_start (GTK_BOX(box), widget, FALSE, FALSE, 0);
    teamlabel = gtk_label_new ("");
    gtk_widget_show (teamlabel);
    gtk_box_pack_start (GTK_BOX(box), teamlabel, FALSE, FALSE, 0);
    infolabel = gtk_label_new ("");
    gtk_label_set_line_wrap (GTK_LABEL(teamlabel), TRUE);
    gtk_widget_show (infolabel);
    gtk_box_pack_start (GTK_BOX(box), infolabel, TRUE, FALSE, 0);

    gtk_container_border_width (GTK_CONTAINER(box), 4);
    gtk_widget_show (box);
    rightbox = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(rightbox), GTK_SHADOW_IN);
    gtk_container_add (GTK_CONTAINER(rightbox), box);
    gtk_widget_show (rightbox);

    /* stuff all the boxes into the table */
    table = gtk_table_new (2, 2, FALSE);
    gtk_table_set_row_spacings (GTK_TABLE(table), 4);
    gtk_table_set_col_spacings (GTK_TABLE(table), 4);
    gtk_container_border_width (GTK_CONTAINER(table), 4);
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
    gchar *nick_utf8, *team_utf8;
  
    if (nick)
    {
      nick_utf8 = g_locale_to_utf8 (nocolor (nick), -1, NULL, NULL, NULL);
      gtk_label_set (GTK_LABEL(namelabel), nick_utf8);
      g_free (nick_utf8);
    }
    else gtk_label_set (GTK_LABEL(namelabel), "");
    if (team)
    {
      team_utf8 = g_locale_to_utf8 (nocolor (team), -1, NULL, NULL, NULL);
      gtk_label_set (GTK_LABEL(teamlabel), team_utf8);
      g_free (team_utf8);
    }
    else gtk_label_set (GTK_LABEL(teamlabel), "");
}

void partyline_status (char *status)
{
    gtk_label_set (GTK_LABEL(infolabel), status);
}

void partyline_text (char *text)
{
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
    gchar *aux1, *aux2; /* for recoding purposes */
    
    /* update the playerlist so that it contains only the given names */
    gtk_list_store_clear (playerlist_model);

    for (i = 0; i < n; i ++) {
        g_snprintf (buf0, sizeof(buf0), "%d", numbers[i]);
        GTET_O_STRCPY (buf1, nocolor(names[i]));
        GTET_O_STRCPY (buf2, nocolor(teams[i]));
        /* we only need to recode buf1 and buf2, buf0 is just a character */
        aux1 = g_locale_to_utf8 (buf1, -1, NULL, NULL, NULL);
        aux2 = g_locale_to_utf8 (buf2, -1, NULL, NULL, NULL);
        gtk_list_store_append (playerlist_model, &iter);
        gtk_list_store_set (playerlist_model, &iter,
                            0, buf0, 1, aux1, 2, aux2, -1);
        g_free (aux1);
        g_free (aux2);
    }

    buf0[0] = buf1[0] = buf2[0] = 0;
    gtk_list_store_append (playerlist_model, &iter);
    gtk_list_store_set (playerlist_model, &iter,
                        0, buf0, 1, buf1, 2, buf2, -1);

    for (i = 0; i < sn; i ++) {
        GTET_O_STRCPY (buf0, "S");
        GTET_O_STRCPY (buf1, nocolor(specs[i]));
        GTET_O_STRCPY (buf2, "");
        /* we only need to recode buf1 and buf2, buf0 is just a character */
        aux1 = g_locale_to_utf8 (buf1, -1, NULL, NULL, NULL);
        aux2 = g_locale_to_utf8 (buf2, -1, NULL, NULL, NULL);
        gtk_list_store_append (playerlist_model, &iter);
        gtk_list_store_set (playerlist_model, &iter,
                            0, buf0, 1, buf1, 2, buf2, -1);
        g_free (aux1);
        g_free (aux2);
    }
}

void partyline_entryfocus (void)
{
    if (connected) gtk_widget_grab_focus (entrybox);
}

void partyline_switch_entryfocus (void)
{ /* FIXME: should only grab when in right notebook */
    if (connected) gtk_widget_grab_focus (entrybox);
}

void textentry (GtkWidget *widget)
{
    const char *text;
    gchar *iso_text;
    text = gtk_entry_get_text (GTK_ENTRY(widget));

    if (strlen(text) == 0) return;
    
    /* convert from UTF-8 to the current locale, will work with ISO8859-1 locales */    
    iso_text = g_locale_from_utf8 (text, -1, NULL, NULL, NULL);

    tetrinet_playerline (iso_text);
    GTET_O_STRCPY (plhistory[plh_end], iso_text);
    gtk_entry_set_text (GTK_ENTRY(widget), "");

    plh_end ++;
    if (plh_end == PLHSIZE) plh_end = 0;
    if (plh_end == plh_start) plh_start ++;
    if (plh_start == PLHSIZE) plh_start = 0;
    plh_cur = plh_end;
    plhistory[plh_cur][0] = 0;
    
    g_free (iso_text);
}

static gint entrykey (GtkWidget *widget, GdkEventKey *key)
{
    int keyval = key->keyval;
    gchar *text;

    if (keyval == GDK_Up || keyval == GDK_Down) {
        if (plh_cur == plh_end) {
            text = g_locale_from_utf8 (gtk_entry_get_text (GTK_ENTRY(widget)), -1, NULL, NULL, NULL);
            GTET_O_STRCPY (plhistory[plh_end], text);
            g_free (text);
        }
        switch (keyval) {
        case GDK_Up:
            if (plh_cur == plh_start) break;
            plh_cur --;
            if (plh_cur == -1) plh_cur = PLHSIZE - 1;
            break;
        case GDK_Down:
            if (plh_cur == plh_end) break;
            plh_cur ++;
            if (plh_cur == PLHSIZE) plh_cur = 0;
            break;
        }
        text = g_locale_to_utf8 (plhistory[plh_cur], -1, NULL, NULL, NULL);
        gtk_entry_set_text (GTK_ENTRY(widget), text);
        gtk_editable_set_position (GTK_EDITABLE (widget), -1);
        g_free (text);
#ifdef DEBUG
        printf ("history: %d %d %d %s\n", plh_start, plh_end, plh_cur,
                plhistory[plh_cur]);
#endif
        gtk_signal_emit_stop_by_name (GTK_OBJECT(widget), "key-press-event");
        return TRUE;
    }
    else if (keyval == GDK_Left || keyval == GDK_Right) {
        return FALSE;
    }
    else {
        plh_cur = plh_end;
        return FALSE;
    }
}
