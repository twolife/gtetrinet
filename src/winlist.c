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

#include "client.h"
#include "tetrinet.h"
#include "winlist.h"
#include "misc.h"

static GtkWidget *winlist;

GtkWidget *winlist_page_new (void)
{
    GtkWidget *align;
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
    GtkListStore *winlist_store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    winlist = gtk_tree_view_new_with_model (GTK_TREE_MODEL (winlist_store));

    /* "T" stands for "Team" here */
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (winlist), -1, _("T"), renderer,
                                                 "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (winlist), -1, _("Name"), renderer,
                                                 "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (winlist), -1, _("Score"), renderer,
                                                 "text", 2, NULL);

    gtk_widget_set_size_request (winlist, 240, 0);

    gtk_widget_show (winlist);
    align = gtk_alignment_new (0.5, 0.5, 0.0, 0.8);
    gtk_container_add (GTK_CONTAINER(align), winlist);
    gtk_container_set_border_width (GTK_CONTAINER(align), 2);

    return align;
}

void winlist_clear (void)
{
    GtkListStore *winlist_model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (winlist)));
  
    gtk_list_store_clear (winlist_model);
}

void winlist_additem (int team, char *name, int score)
{
    GtkListStore *winlist_model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (winlist)));
    GtkTreeIter iter;
    char buf[16], *item[3];
    gchar *name_utf8;

    if (team) item[0] = "T";
    else item[0] = "";
    item[1] = nocolor (name);
    name_utf8 = g_locale_to_utf8 (item[1], -1, NULL, NULL, NULL);
    g_snprintf (buf, sizeof(buf), "%d", score);
    item[2] = buf;

    gtk_list_store_append (winlist_model, &iter);
    gtk_list_store_set (winlist_model, &iter,
                        0, item[0],
                        1, name_utf8,
                        2, item[2],
                        -1);
    g_free (name_utf8);
}

void winlist_focus (void)
{
  gtk_widget_grab_focus (winlist);
}
