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

#include "client.h"
#include "tetrinet.h"
#include "partyline.h"
#include "misc.h"
#include "commands.h"

/* for the player list */
static char *listtitles[] = {
    "",
    N_("Name"),
    N_("Team")
};

/* widgets that we have to do stuff with */
static GtkWidget *playerlist, *textbox, *entrybox,
    *namelabel, *teamlabel, *infolabel, *textboxscroll;

/* some more widgets for layout */
static GtkWidget *table, *leftbox, *rightbox;

/* stuff for pline history */
#define PLHSIZE 64
char plhistory[PLHSIZE][256];
int plh_start = 0, plh_end = 0, plh_cur = 0;

/* function prototypes for callbacks */
static void textentry (GtkWidget *widget, gpointer data);
static gint entrykey (GtkWidget *widget, GdkEventKey *key);

GtkWidget *partyline_page_new (void)
{
    GtkWidget *widget, *box; /* generic temp variables */

    /* left box */
    leftbox = gtk_vbox_new (FALSE, 4);
    /* chat thingy */
    /* textbox with scrollbars */
    textbox = gtk_text_new (NULL, NULL);
    gtk_text_set_word_wrap (GTK_TEXT(textbox), TRUE);
    GTK_WIDGET_UNSET_FLAGS(textbox, GTK_CAN_FOCUS);
    gtk_signal_connect (GTK_OBJECT(textbox), "button_press_event",
                        GTK_SIGNAL_FUNC(partyline_entryfocus), NULL);
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
    gtk_signal_connect (GTK_OBJECT(entrybox), "activate",
                        GTK_SIGNAL_FUNC(textentry), NULL);
    gtk_signal_connect (GTK_OBJECT(entrybox), "key_press_event",
                        GTK_SIGNAL_FUNC(entrykey), NULL);
    gtk_widget_show (entrybox);
    gtk_box_pack_start (GTK_BOX(leftbox), entrybox, FALSE, FALSE, 0);
    gtk_widget_show (leftbox);

    /* player list */
    playerlist = gtk_clist_new_with_titles (3, listtitles);
    gtk_clist_set_column_width (GTK_CLIST(playerlist), 0, 10);
    gtk_clist_set_column_width (GTK_CLIST(playerlist), 1, 60);
    gtk_clist_column_titles_passive (GTK_CLIST(playerlist));
    gtk_widget_set_usize (playerlist, 150, 200);
    gtk_widget_show (playerlist);

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
    gtk_table_attach (GTK_TABLE(table), playerlist, 1, 2, 0, 1,
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
    if (nick) gtk_label_set (GTK_LABEL(namelabel), nocolor(nick));
    else gtk_label_set (GTK_LABEL(namelabel), "");
    if (team) gtk_label_set (GTK_LABEL(teamlabel), nocolor(team));
    else gtk_label_set (GTK_LABEL(teamlabel), "");
}

void partyline_status (char *status)
{
    gtk_label_set (GTK_LABEL(infolabel), status);
}

void partyline_text (char *text)
{
    textbox_addtext (GTK_TEXT(textbox), text);
}

void partyline_playerlist (int *numbers, char **names, char **teams, int n, char **specs, int sn)
{
    int i;
    char buf0[16], buf1[128], buf2[128], *item[3] = {buf0, buf1, buf2};
    /* update the playerlist so that it contains only the given names */
    gtk_clist_freeze (GTK_CLIST(playerlist));
    gtk_clist_clear (GTK_CLIST(playerlist));
    for (i = 0; i < n; i ++) {
        sprintf (item[0], "%d", numbers[i]);
        strcpy (item[1], nocolor(names[i]));
        strcpy (item[2], nocolor(teams[i]));
        gtk_clist_append (GTK_CLIST(playerlist), item);
    }
    buf0[0] = buf1[0] = buf2[0] = 0;
    gtk_clist_append (GTK_CLIST(playerlist), item);
    for (i = 0; i < sn; i ++) {
        strcpy (item[0], "S");
        strcpy (item[1], nocolor(specs[i]));
        strcpy (item[2], "");
        gtk_clist_append (GTK_CLIST(playerlist), item);
    }
    gtk_clist_thaw (GTK_CLIST(playerlist));
}

void partyline_entryfocus (void)
{
    if (connected) gtk_widget_grab_focus (entrybox);
}

void partyline_switch_entryfocus (void)
{
    if (connected) gtk_widget_grab_focus (entrybox);
}

void textentry (GtkWidget *widget, gpointer data)
{
    char *text;
    text = gtk_entry_get_text (GTK_ENTRY(widget));

    if (strlen(text) == 0) return;

    tetrinet_playerline (text);
    strcpy (plhistory[plh_end], text);
    gtk_entry_set_text (GTK_ENTRY(widget), "");

    plh_end ++;
    if (plh_end == PLHSIZE) plh_end = 0;
    if (plh_end == plh_start) plh_start ++;
    if (plh_start == PLHSIZE) plh_start = 0;
    plh_cur = plh_end;
    plhistory[plh_cur][0] = 0;
}

static gint entrykey (GtkWidget *widget, GdkEventKey *key)
{
    int keyval = key->keyval;

    if (keyval == GDK_Up || keyval == GDK_Down) {
        if (plh_cur == plh_end) {
            char *text;
            text = gtk_entry_get_text (GTK_ENTRY(widget));
            strcpy (plhistory[plh_end], text);
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
        gtk_entry_set_text (GTK_ENTRY(widget), plhistory[plh_cur]);
#ifdef DEBUG
        printf ("history: %d %d %d %s\n", plh_start, plh_end, plh_cur,
                plhistory[plh_cur]);
#endif
        gtk_signal_emit_stop_by_name (GTK_OBJECT(widget), "key_press_event");
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
