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

typedef struct _WinlistItem {
    GObject parent_instance;

    gboolean team;
    char *name;
    int score;
} WinlistItem;

typedef struct _WinlistItemClass {
    GObjectClass parent_class;
} WinlistItemClass;

G_DEFINE_TYPE (WinlistItem, winlist_item, G_TYPE_OBJECT)

static GtkWidget *winlist;
static GListStore *winlist_store;
static GdkTexture *team_icon, *alone_icon;

static void
winlist_item_finalize (GObject *object)
{
    WinlistItem *item = (WinlistItem *) object;

    g_free (item->name);

    G_OBJECT_CLASS (winlist_item_parent_class)->finalize (object);
}

static void
winlist_item_class_init (WinlistItemClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = winlist_item_finalize;
}

static void
winlist_item_init (WinlistItem *item)
{
    item->team = FALSE;
    item->name = NULL;
    item->score = 0;
}

static WinlistItem *
winlist_item_new (gboolean team, const char *name, int score)
{
    WinlistItem *item;

    item = g_object_new (winlist_item_get_type (), NULL);
    item->team = team;
    item->name = g_strdup (name);
    item->score = score;

    return item;
}

static void
icon_factory_setup (GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                    GtkListItem *list_item,
                    gpointer data G_GNUC_UNUSED)
{
    GtkWidget *image = gtk_image_new ();

    gtk_image_set_pixel_size (GTK_IMAGE (image), 24);
    gtk_list_item_set_child (list_item, image);
}

static void
icon_factory_bind (GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                   GtkListItem *list_item,
                   gpointer data G_GNUC_UNUSED)
{
    WinlistItem *item = gtk_list_item_get_item (list_item);
    GtkWidget *image = gtk_list_item_get_child (list_item);
    GdkTexture *texture = item->team ? team_icon : alone_icon;

    gtk_image_set_from_paintable (GTK_IMAGE (image),
                                  texture != NULL ? GDK_PAINTABLE (texture) : NULL);
}

static void
name_factory_setup (GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                    GtkListItem *list_item,
                    gpointer data G_GNUC_UNUSED)
{
    GtkWidget *label = gtk_label_new (NULL);

    gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
    gtk_list_item_set_child (list_item, label);
}

static void
name_factory_bind (GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                   GtkListItem *list_item,
                   gpointer data G_GNUC_UNUSED)
{
    WinlistItem *item = gtk_list_item_get_item (list_item);
    GtkWidget *label = gtk_list_item_get_child (list_item);

    gtk_label_set_text (GTK_LABEL (label), item->name);
}

static void
score_factory_setup (GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                     GtkListItem *list_item,
                     gpointer data G_GNUC_UNUSED)
{
    GtkWidget *label = gtk_label_new (NULL);

    gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
    gtk_list_item_set_child (list_item, label);
}

static void
score_factory_bind (GtkSignalListItemFactory *factory G_GNUC_UNUSED,
                    GtkListItem *list_item,
                    gpointer data G_GNUC_UNUSED)
{
    WinlistItem *item = gtk_list_item_get_item (list_item);
    GtkWidget *label = gtk_list_item_get_child (list_item);
    char buf[16];

    g_snprintf (buf, sizeof (buf), "%d", item->score);
    gtk_label_set_text (GTK_LABEL (label), buf);
}

static GtkColumnViewColumn *
winlist_column_new (const char *title,
                    GCallback setup_cb,
                    GCallback bind_cb)
{
    GtkListItemFactory *factory;
    GtkColumnViewColumn *column;

    factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup", setup_cb, NULL);
    g_signal_connect (factory, "bind", bind_cb, NULL);

    column = gtk_column_view_column_new (title, factory);
    gtk_column_view_column_set_resizable (column, TRUE);

    return column;
}

GtkWidget *winlist_page_new (void)
{
    GtkWidget *scroll;
    GtkSelectionModel *selection;
    GtkColumnViewColumn *column;
    GError *error = NULL;

    team_icon = gdk_texture_new_from_filename (GTETPIXMAPSDIR "/team.png",
                                               &error);
    if (team_icon == NULL) {
        g_warning ("Unable to load team icon: %s", error->message);
        g_clear_error (&error);
    }

    alone_icon = gdk_texture_new_from_filename (GTETPIXMAPSDIR "/alone.png",
                                                &error);
    if (alone_icon == NULL) {
        g_warning ("Unable to load player icon: %s", error->message);
        g_clear_error (&error);
    }

    winlist_store = g_list_store_new (winlist_item_get_type ());
    selection = GTK_SELECTION_MODEL (
        gtk_no_selection_new (G_LIST_MODEL (g_object_ref (winlist_store))));

    winlist = gtk_column_view_new (selection);
    gtk_widget_set_focusable (winlist, TRUE);

    column = winlist_column_new (_("T"),
                                 G_CALLBACK (icon_factory_setup),
                                 G_CALLBACK (icon_factory_bind));
    gtk_column_view_append_column (GTK_COLUMN_VIEW (winlist), column);

    column = winlist_column_new (_("Name"),
                                 G_CALLBACK (name_factory_setup),
                                 G_CALLBACK (name_factory_bind));
    gtk_column_view_append_column (GTK_COLUMN_VIEW (winlist), column);

    column = winlist_column_new (_("Score"),
                                 G_CALLBACK (score_factory_setup),
                                 G_CALLBACK (score_factory_bind));
    gtk_column_view_append_column (GTK_COLUMN_VIEW (winlist), column);

    scroll = gtk_scrolled_window_new ();
    gtk_widget_set_hexpand (scroll, TRUE);
    gtk_widget_set_vexpand (scroll, TRUE);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), winlist);

    return scroll;
}

void winlist_clear (void)
{
    g_list_store_remove_all (winlist_store);
}

void winlist_additem (int team, char *name, int score)
{
    WinlistItem *item;
    char *clean_name;

    clean_name = nocolor (name);
    item = winlist_item_new (team != 0, clean_name, score);

    g_list_store_append (winlist_store, item);
    g_object_unref (item);
}

void winlist_focus (void)
{
    gtk_widget_grab_focus (winlist);
}
