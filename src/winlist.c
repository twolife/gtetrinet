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
#include <string.h>
#include <stdio.h>

#include "client.h"
#include "tetrinet.h"
#include "winlist.h"
#include "misc.h"

static GtkWidget *winlist;
static GdkPixbuf *team_icon, *alone_icon;

GtkWidget *winlist_page_new (void)
{
    GtkWidget *align, *scroll;
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
    GtkCellRenderer *pixbuf_renderer = gtk_cell_renderer_pixbuf_new ();
    GtkListStore *winlist_store = gtk_list_store_new (3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
    GdkPixbuf *pixbuf;
    GtkBuilder *builder;

    /* Load the icons and scale them */
    pixbuf = gdk_pixbuf_new_from_file (GTETPIXMAPSDIR "/team.png",  NULL);
    team_icon = gdk_pixbuf_scale_simple (pixbuf, 24, 24, GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);
    
    pixbuf = gdk_pixbuf_new_from_file (GTETPIXMAPSDIR "/alone.png", NULL);
    alone_icon = gdk_pixbuf_scale_simple (pixbuf, 24, 24, GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);

    builder = gtk_builder_new_from_resource("/org/gtetrinet/winlist.ui");
    winlist = gtk_builder_get_object(builder, "winlist");
    winlist_store = gtk_builder_get_object(builder, "winlist_model");

    return gtk_builder_get_object(builder, "winlist_parent");
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
    char buf[16], *item[2];
    GdkPixbuf *pixbuf;

    if (team) pixbuf = team_icon;
    else pixbuf = alone_icon;
    item[0] = nocolor (name);
    g_snprintf (buf, sizeof(buf), "%d", score);
    item[1] = buf;

    gtk_list_store_append (winlist_model, &iter);
    gtk_list_store_set (winlist_model, &iter,
                        0, pixbuf,
                        1, item[0],
                        2, item[1],
                        -1);
}

void winlist_focus (void)
{
  gtk_widget_grab_focus (winlist);
}
