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
#include "winlist.h"
#include "misc.h"

char *winlisttitles[3] = {
    N_("T"),
    N_("Name"),
    N_("Score")
};

GtkWidget *winlist;

GtkWidget *winlist_page_new (void)
{
    GtkWidget *align;

    winlist = gtk_clist_new_with_titles (3, winlisttitles);

    gtk_clist_set_column_width (GTK_CLIST(winlist), 0, 8);
    gtk_clist_set_column_width (GTK_CLIST(winlist), 1, 120);
    gtk_widget_set_usize (winlist, 240, 0);

    gtk_clist_column_titles_passive (GTK_CLIST(winlist));

    gtk_widget_show (winlist);
    align = gtk_alignment_new (0.5, 0.5, 0.0, 0.8);
    gtk_container_add (GTK_CONTAINER(align), winlist);
    gtk_container_border_width (GTK_CONTAINER(align), 2);

    return align;
}

void winlist_clear (void)
{
    gtk_clist_clear (GTK_CLIST(winlist));
}

void winlist_additem (int team, char *name, int score)
{
    char buf[16], *item[3];
    if (team) item[0] = "T";
    else item[0] = "";
    item[1] = nocolor (name);
    sprintf (buf, "%d", score);
    item[2] = buf;

    gtk_clist_append (GTK_CLIST(winlist), item);
}
