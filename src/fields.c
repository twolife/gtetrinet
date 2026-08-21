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
#include <stdio.h>
#include <stdlib.h>
#include <cairo-gobject.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gtet_config.h"
#include "client.h"
#include "tetrinet.h"
#include "tetris.h"
#include "fields.h"
#include "misc.h"
#include "gtetrinet.h"
#include "string.h"

#define BLOCKSIZE bsize
#define SMALLBLOCKSIZE (BLOCKSIZE/2)

static GtkWidget *nextpiecewidget,
    *specialwidget, *speciallabel, *attdefwidget, *lineswidget, *levelwidget,
    *activewidget, *activelabel, *gmsgtext, *gmsginput, *fieldspage, *pagecontents;

static GtkWidget *fieldwidgets[6];
static GtkWidget *fieldnumber_widgets[6];
static GtkWidget *fieldnumber_separator_widgets[6];
static GtkWidget *playername_widgets[6];
static GtkWidget *single_description_widgets[6];
static GtkWidget *teamname_separator_widgets[6];
static GtkWidget *teamname_widgets[6];

static GtkWidget *fields_page_contents (void);
static GtkWidget *fields_create_player_field (int playernb);

static void fields_draw (GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer field);
static void fields_nextpiece_draw (GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data);
static void fields_specials_draw (GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data);

static void fields_refreshfield (cairo_t *cr, int field);
static void fields_drawblock (cairo_t *cr, int field, int x, int y, char block);
static void fields_rendernextblock (cairo_t *cr, TETRISBLOCK block);
static void fields_renderspecials (cairo_t *cr);
static void drawpix (cairo_t *cr, int srcx, int srcy, int destx, int desty, int width, int height);

static void gmsginput_activate (void);

static cairo_surface_t *blockpix;

static GdkCursor *invisible_cursor, *arrow_cursor;

static FIELD displayfields[6]; /* what is actually displayed */
static TETRISBLOCK displayblock;

void fields_init (void)
{
    GtkAlertDialog *dialog;
    GdkPixbuf *pb = NULL;
    GError *err = NULL;
    cairo_surface_t *mask = NULL;

    if (!(pb = gdk_pixbuf_new_from_file(blocksfile, &err))) {
        dialog = gtk_alert_dialog_new ("%s",
                                     _("Error loading theme: cannot load graphics file\n"
                                       "Falling back to default"));
        gtk_alert_dialog_show (dialog, NULL);
        g_object_unref (dialog);
        g_string_assign(currenttheme, DEFAULTTHEME);
        config_loadtheme (DEFAULTTHEME);
        err = NULL;
        if (!(pb = gdk_pixbuf_new_from_file(blocksfile, &err))) {
            /* shouldnt happen */
            fprintf (stderr, _("Error loading default theme: Aborting...\n"
                               "Check for installation errors\n"));
            exit (0);
        }
    }

    blockpix = cairo_image_surface_create (CAIRO_FORMAT_RGB24, gdk_pixbuf_get_width (pb), gdk_pixbuf_get_height (pb));
    cairo_t *cr = cairo_create (blockpix);
    gdk_cairo_set_source_pixbuf (cr, pb, 0, 0);
    cairo_paint (cr);
    cairo_destroy (cr);
    g_object_unref (pb);
}

void fields_cleanup (void)
{
    if (blockpix) {
        cairo_surface_destroy (blockpix);
        blockpix = NULL;
    }
}

/* a mess of functions here for creating the fields page */

GtkWidget *fields_page_new (void)
{
    pagecontents = fields_page_contents ();

    if (fieldspage == NULL) {
        fieldspage = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_margin_start (fieldspage, 2);
        gtk_widget_set_margin_end (fieldspage, 2);
        gtk_widget_set_margin_top (fieldspage, 2);
        gtk_widget_set_margin_bottom (fieldspage, 2);
    }
    gtk_box_append (GTK_BOX(fieldspage), pagecontents);

    /* create the cursors */
    //bitmap = gdk_bitmap_create_from_data (gtk_widget_get_window(GTK_WIDGET (fieldspage)), "\0", 1, 1);
    //invisible_cursor = gdk_cursor_new_from_pixmap (bitmap, bitmap, &black, &black, 0, 0);
    //arrow_cursor = gdk_cursor_new (GDK_X_CURSOR);

    return fieldspage;
}

void fields_page_destroy_contents (void)
{
    if (pagecontents) {
        gtk_widget_unparent (pagecontents);
        pagecontents = NULL;
    }
}

static GtkWidget *
fields_create_player_field (int playernb)
{
    GtkWidget *frame;
    GtkWidget *outer_grid;
    GtkWidget *labels_grid;
    GtkWidget *fieldwidget;
    GtkWidget *widget;
    int blocksize;

    blocksize = (playernb == 0) ? BLOCKSIZE : SMALLBLOCKSIZE;

    frame = gtk_frame_new (NULL);
    outer_grid = gtk_grid_new ();
    labels_grid = gtk_grid_new ();

    gtk_frame_set_child (GTK_FRAME(frame), outer_grid);
    gtk_grid_attach (GTK_GRID(outer_grid), labels_grid, 0, 0, 1, 1);

    fieldnumber_widgets[playernb] = gtk_label_new ("");
    widget = fieldnumber_widgets[playernb];
    gtk_widget_set_margin_start (widget, 2);
    gtk_widget_set_margin_end (widget, 2);
    gtk_widget_set_margin_top (widget, 2);
    gtk_widget_set_margin_bottom (widget, 2);
    gtk_grid_attach (GTK_GRID(labels_grid), widget, 0, 0, 1, 1);

    fieldnumber_separator_widgets[playernb] =
        gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_grid_attach (GTK_GRID(labels_grid),
                     fieldnumber_separator_widgets[playernb],
                     1, 0, 1, 1);

    playername_widgets[playernb] = gtk_label_new ("");
    widget = playername_widgets[playernb];
    gtk_widget_set_margin_start (widget, 2);
    gtk_widget_set_margin_end (widget, 2);
    gtk_widget_set_margin_top (widget, 2);
    gtk_widget_set_margin_bottom (widget, 2);
    gtk_grid_attach (GTK_GRID(labels_grid), widget, 2, 0, 1, 1);

    single_description_widgets[playernb] = gtk_label_new (_("Not playing"));
    gtk_widget_set_hexpand (single_description_widgets[playernb], TRUE);
    gtk_grid_attach (GTK_GRID(labels_grid),
                     single_description_widgets[playernb],
                     3, 0, 1, 1);

    teamname_separator_widgets[playernb] =
        gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    widget = teamname_separator_widgets[playernb];
    gtk_widget_set_margin_start (widget, 2);
    gtk_widget_set_margin_end (widget, 2);
    gtk_widget_set_margin_top (widget, 2);
    gtk_widget_set_margin_bottom (widget, 2);
    gtk_grid_attach (GTK_GRID(labels_grid), widget, 4, 0, 1, 1);

    teamname_widgets[playernb] = gtk_label_new ("");
    gtk_grid_attach (GTK_GRID(labels_grid),
                     teamname_widgets[playernb],
                     5, 0, 1, 1);

    fieldwidget = gtk_drawing_area_new ();
    fieldwidgets[playernb] = fieldwidget;
    gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA(fieldwidget),
                                    fields_draw,
                                    GINT_TO_POINTER(playernb),
                                    NULL);
    gtk_widget_set_size_request (fieldwidget,
                                 blocksize * FIELDWIDTH,
                                 blocksize * FIELDHEIGHT);
    gtk_grid_attach (GTK_GRID(outer_grid), fieldwidget, 0, 1, 1, 1);

    fields_setlabel (playernb, NULL, NULL, 0);

    return frame;
}

GtkWidget *fields_page_contents (void)
{
    GtkWidget *fieldsparent;
    GtkWidget *fieldslots[6];
    GtkWidget *grid;
    GtkWidget *frame;
    GtkWidget *label;
    GtkWidget *scroll;
    GtkWidget *messages_grid;
    GtkWidget *specials_grid;
    GtkWidget *stuffalign;
    int playernb;

    fieldsparent = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID(fieldsparent), 2);
    gtk_grid_set_column_spacing (GTK_GRID(fieldsparent), 2);

    /*
     * Field placement matches the old fields.ui:
     *
     *   local | next | 1 | 2 | 3
     *         | att/def |   | 4 | 5
     *   specials
     *   game messages
     */
    fieldslots[0] = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_grid_attach (GTK_GRID(fieldsparent), fieldslots[0], 0, 0, 1, 2);

    fieldslots[1] = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_grid_attach (GTK_GRID(fieldsparent), fieldslots[1], 2, 0, 1, 1);

    fieldslots[2] = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_grid_attach (GTK_GRID(fieldsparent), fieldslots[2], 3, 0, 1, 1);

    fieldslots[3] = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_grid_attach (GTK_GRID(fieldsparent), fieldslots[3], 4, 0, 1, 1);

    fieldslots[4] = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_grid_attach (GTK_GRID(fieldsparent), fieldslots[4], 3, 1, 1, 2);

    fieldslots[5] = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_grid_attach (GTK_GRID(fieldsparent), fieldslots[5], 4, 1, 1, 2);

    for (playernb = 0; playernb < 6; playernb++)
        gtk_box_append (GTK_BOX(fieldslots[playernb]),
                        fields_create_player_field (playernb));

    /* Next piece + line/level status. */
    stuffalign = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request (stuffalign, BLOCKSIZE*6, BLOCKSIZE*11);
    gtk_grid_attach (GTK_GRID(fieldsparent), stuffalign, 1, 0, 1, 1);

    grid = gtk_grid_new ();
    gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (grid, GTK_ALIGN_CENTER);
    gtk_box_append (GTK_BOX(stuffalign), grid);

    label = gtk_label_new (_("Next piece:"));
    gtk_label_set_xalign (GTK_LABEL(label), 0.0f);
    gtk_grid_attach (GTK_GRID(grid), label, 0, 0, 1, 1);

    nextpiecewidget = gtk_drawing_area_new ();
    gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA(nextpiecewidget),
                                    fields_nextpiece_draw, NULL, NULL);
    gtk_widget_set_size_request (nextpiecewidget,
                                 BLOCKSIZE*9/2, BLOCKSIZE*9/2);
    frame = gtk_frame_new (NULL);
    gtk_frame_set_child (GTK_FRAME(frame), nextpiecewidget);
    gtk_grid_attach (GTK_GRID(grid), frame, 0, 1, 1, 1);

    {
        GtkWidget *stats = gtk_grid_new ();

        gtk_grid_set_column_spacing (GTK_GRID(stats), 5);

        label = gtk_label_new (_("Lines:"));
        gtk_label_set_xalign (GTK_LABEL(label), 0.0f);
        gtk_grid_attach (GTK_GRID(stats), label, 0, 0, 1, 1);

        lineswidget = gtk_label_new ("");
        gtk_label_set_xalign (GTK_LABEL(lineswidget), 0.0f);
        gtk_grid_attach (GTK_GRID(stats), lineswidget, 1, 0, 1, 1);

        label = gtk_label_new ("");
        gtk_label_set_xalign (GTK_LABEL(label), 0.0f);
        gtk_grid_attach (GTK_GRID(stats), label, 0, 1, 1, 1);

        label = gtk_label_new (_("Level:"));
        gtk_label_set_xalign (GTK_LABEL(label), 0.0f);
        gtk_grid_attach (GTK_GRID(stats), label, 0, 2, 1, 1);

        levelwidget = gtk_label_new ("");
        gtk_label_set_xalign (GTK_LABEL(levelwidget), 0.0f);
        gtk_grid_attach (GTK_GRID(stats), levelwidget, 1, 2, 1, 1);

        activelabel = gtk_label_new (_("Active level:"));
        gtk_label_set_xalign (GTK_LABEL(activelabel), 0.0f);
        gtk_grid_attach (GTK_GRID(stats), activelabel, 0, 3, 1, 1);

        activewidget = gtk_label_new ("");
        gtk_label_set_xalign (GTK_LABEL(activewidget), 0.0f);
        gtk_grid_attach (GTK_GRID(stats), activewidget, 1, 3, 1, 1);

        gtk_grid_attach (GTK_GRID(grid), stats, 0, 2, 1, 1);
    }

    /* Attacks and defenses. */
    grid = gtk_grid_new ();
    gtk_grid_attach (GTK_GRID(fieldsparent), grid, 1, 1, 2, 1);

    label = gtk_label_new (_("Attacks and defenses:"));
    gtk_grid_attach (GTK_GRID(grid), label, 0, 0, 1, 1);

    attdefwidget = gtk_text_view_new ();
    gtk_text_view_set_editable (GTK_TEXT_VIEW(attdefwidget), FALSE);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW(attdefwidget), GTK_WRAP_WORD);
    gtk_text_view_set_buffer (GTK_TEXT_VIEW(attdefwidget),
                              gtk_text_buffer_new(tag_table));
    gtk_widget_set_size_request (attdefwidget,
                                 MAX(22*12, BLOCKSIZE*12),
                                 BLOCKSIZE*10);

    scroll = gtk_scrolled_window_new ();
    gtk_widget_set_focusable (scroll, TRUE);
    gtk_widget_set_hexpand (scroll, TRUE);
    gtk_widget_set_vexpand (scroll, TRUE);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW(scroll), attdefwidget);
    gtk_grid_attach (GTK_GRID(grid), scroll, 0, 1, 1, 1);

    /* Specials. */
    specials_grid = gtk_grid_new ();
    gtk_widget_set_halign (specials_grid, GTK_ALIGN_END);
    gtk_grid_attach (GTK_GRID(fieldsparent), specials_grid, 0, 2, 3, 1);

    speciallabel = gtk_label_new ("");
    gtk_widget_set_hexpand (speciallabel, TRUE);
    gtk_grid_attach (GTK_GRID(specials_grid), speciallabel, 0, 0, 1, 1);

    specialwidget = gtk_drawing_area_new ();
    gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA(specialwidget),
                                    fields_specials_draw, NULL, NULL);
    gtk_widget_set_size_request (specialwidget, BLOCKSIZE*18, BLOCKSIZE);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_child (GTK_FRAME(frame), specialwidget);
    gtk_grid_attach (GTK_GRID(specials_grid), frame, 1, 0, 1, 1);
    fields_setspeciallabel (NULL);

    /* Game messages and input. */
    messages_grid = gtk_grid_new ();
    gtk_widget_set_vexpand (messages_grid, TRUE);
    gtk_grid_attach (GTK_GRID(fieldsparent), messages_grid, 0, 3, 5, 1);

    gmsgtext = gtk_text_view_new ();
    gtk_widget_set_vexpand (gmsgtext, TRUE);
    gtk_text_view_set_editable (GTK_TEXT_VIEW(gmsgtext), FALSE);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW(gmsgtext), GTK_WRAP_WORD);
    gtk_text_view_set_buffer (GTK_TEXT_VIEW(gmsgtext),
                              gtk_text_buffer_new(tag_table));

    scroll = gtk_scrolled_window_new ();
    gtk_widget_set_hexpand (scroll, TRUE);
    gtk_widget_set_vexpand (scroll, TRUE);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW(scroll), gmsgtext);
    gtk_grid_attach (GTK_GRID(messages_grid), scroll, 0, 0, 1, 1);

    gmsginput = gtk_entry_new ();
    gtk_widget_set_focusable (gmsginput, TRUE);
    gtk_entry_set_max_length (GTK_ENTRY(gmsginput), 128);
    g_signal_connect (gmsginput, "activate",
                      G_CALLBACK(gmsginput_activate), NULL);
    gtk_grid_attach (GTK_GRID(messages_grid), gmsginput, 0, 1, 1, 1);

    fields_setlines (-1);
    fields_setlevel (-1);
    fields_setactivelevel (-1);
    fields_gmsginput (FALSE);

    return fieldsparent;
}

void fields_draw (GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer field)
{
    GtkWidget *widget = GTK_WIDGET (area);
    (void)width;
    (void)height;

    fields_refreshfield (cr, GPOINTER_TO_INT (field));

    /* hide the cursor */
    gtk_widget_set_cursor (widget, ingame ? invisible_cursor : arrow_cursor);
}

void fields_refreshfield (cairo_t *cr, int field)
{
    int x, y;
    for (y = 0; y < FIELDHEIGHT; y ++)
        for (x = 0; x < FIELDWIDTH; x ++)
            fields_drawblock (cr, field, x, y, displayfields[field][y][x]);
}

void fields_drawfield (int field, FIELD newfield)
{
    memcpy (displayfields[field], newfield, sizeof (displayfields[field]));

    if (fieldwidgets[field] != NULL)
        gtk_widget_queue_draw (fieldwidgets[field]);
}

void drawpix(cairo_t *cr, int srcx, int srcy, int destx, int desty, int width, int height)
{
    cairo_save (cr);
    cairo_set_source_surface (cr, blockpix, -srcx+destx, -srcy+desty); // move big image, so the block we need is in the right position on cr
    cairo_rectangle (cr, destx, desty, width, height); // only draw the block we need
    cairo_fill(cr);
    cairo_restore (cr);
}

void fields_drawblock (cairo_t *cr, int field, int x, int y, char block)
{
    int srcx, srcy, destx, desty, blocksize;

    if (field == 0) {
        blocksize = BLOCKSIZE;
        if (block == 0) {
            srcx = blocksize*x;
            srcy = BLOCKSIZE+SMALLBLOCKSIZE + blocksize*y;
        }
        else {
            srcx = (block-1) * blocksize;
            srcy = 0;
        }
    }
    else {
        blocksize = SMALLBLOCKSIZE;
        if (block == 0) {
            srcx = BLOCKSIZE*FIELDWIDTH + blocksize*x;
            srcy = BLOCKSIZE+SMALLBLOCKSIZE + blocksize*y;
        }
        else {
            srcx = (block-1) * blocksize;
            srcy = BLOCKSIZE;
        }
    }
    destx = blocksize * x;
    desty = blocksize * y;

/*    gdk_draw_drawable (fieldwidgets[field]->window,
                       fieldwidgets[field]->style->black_gc,
                       blockpix, srcx, srcy, destx, desty,
                       blocksize, blocksize);*/
    drawpix(cr, srcx, srcy, destx, desty, blocksize, blocksize);
}

void fields_setlabel (int field, char *name, char *team, int num)
{
    char buf[11];

    g_snprintf (buf, sizeof(buf), "%d", num);

    if (name == NULL) {
        gtk_widget_set_visible (fieldnumber_widgets[field], FALSE);
        gtk_widget_set_visible (fieldnumber_separator_widgets[field], FALSE);
        gtk_widget_set_visible (playername_widgets[field], FALSE);
        gtk_widget_set_visible (single_description_widgets[field], TRUE);
        gtk_widget_set_visible (teamname_separator_widgets[field], FALSE);
        gtk_widget_set_visible (teamname_widgets[field], FALSE);

        gtk_label_set_text (GTK_LABEL(fieldnumber_widgets[field]), "");
        gtk_label_set_text (GTK_LABEL(playername_widgets[field]), "");
        gtk_label_set_text (GTK_LABEL(single_description_widgets[field]),
                            _("Not playing"));
        gtk_label_set_text (GTK_LABEL(teamname_widgets[field]), "");
    }
    else {
        gtk_widget_set_visible (fieldnumber_widgets[field], TRUE);
        gtk_widget_set_visible (fieldnumber_separator_widgets[field], TRUE);
        gtk_widget_set_visible (playername_widgets[field], TRUE);
        gtk_widget_set_visible (single_description_widgets[field], FALSE);

        gtk_label_set_text (GTK_LABEL(fieldnumber_widgets[field]), buf);
        gtk_label_set_text (GTK_LABEL(playername_widgets[field]), name);
        gtk_label_set_text (GTK_LABEL(single_description_widgets[field]), "");

        if (team == NULL || team[0] == 0) {
            gtk_widget_set_visible (teamname_separator_widgets[field], FALSE);
            gtk_widget_set_visible (teamname_widgets[field], FALSE);
            gtk_label_set_text (GTK_LABEL(teamname_widgets[field]), "");
        }
        else {
            gtk_widget_set_visible (teamname_separator_widgets[field], TRUE);
            gtk_widget_set_visible (teamname_widgets[field], TRUE);
            gtk_label_set_text (GTK_LABEL(teamname_widgets[field]), team);
        }
    }
}

void fields_setspeciallabel (char *label)
{
    if (label == NULL) {
        gtk_label_set_text (GTK_LABEL(speciallabel), _("Specials:"));
    }
    else {
        gtk_label_set_text (GTK_LABEL(speciallabel), label);
    }
}

void fields_nextpiece_draw (GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET (area);
    (void)width;
    (void)height;
    (void)data;

    fields_rendernextblock (cr, displayblock);
    gtk_widget_set_cursor (widget, ingame ? invisible_cursor : arrow_cursor);
}

void fields_specials_draw (GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data)
{
    GtkWidget *widget = GTK_WIDGET (area);
    (void)width;
    (void)height;
    (void)data;

    fields_renderspecials (cr);
    gtk_widget_set_cursor (widget, ingame ? invisible_cursor : arrow_cursor);
}

void fields_drawspecials (void)
{
    if (specialwidget)
        gtk_widget_queue_draw (specialwidget);
}
void fields_renderspecials (cairo_t *cr)
{
    int i;

    cairo_save (cr);
    cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
    cairo_paint (cr);
    cairo_restore (cr);

    for (i = 0; i < 18; i ++) {
        if (i < specialblocknum) {
/*            gdk_draw_drawable (specialwidget->window,
                               specialwidget->style->black_gc,
                               blockpix, (specialblocks[i]-1)*BLOCKSIZE,
                               0, BLOCKSIZE*i, 0, BLOCKSIZE, BLOCKSIZE);*/
              drawpix (cr, (specialblocks[i]-1)*BLOCKSIZE, 0, BLOCKSIZE*i, 0, BLOCKSIZE, BLOCKSIZE);
        }
    }
}

void fields_drawnextblock (TETRISBLOCK block)
{
    if (block != NULL)
        memcpy (displayblock, block, sizeof (displayblock));
    if (nextpiecewidget)
        gtk_widget_queue_draw (nextpiecewidget);
}
void fields_rendernextblock (cairo_t *cr, TETRISBLOCK block)
{
    int x, y, xstart = 4, ystart = 4, xpos, ypos;
    // Draw the black background
    /*gdk_draw_rectangle (nextpiecewidget->window, nextpiecewidget->style->black_gc,
                        TRUE, 0, 0, BLOCKSIZE*9/2, BLOCKSIZE*9/2);*/
    cairo_save (cr);
    cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
    cairo_paint (cr);
    cairo_restore (cr);
    for (y = 0; y < 4; y ++)
        for (x = 0; x < 4; x ++)
            if (block[y][x]) {
                if (y < ystart) ystart = y;
                if (x < xstart) xstart = x;
            }
    for (y = ystart; y < 4; y ++)
        for (x = xstart; x < 4; x ++) {
            if (block[y][x]) {
/*                gdk_draw_drawable (gtk_widget_get_window(nextpiecewidget),
                                   gtk_widget_get_style(nextpiecewidget)->black_gc,
                                   blockpix, (block[y][x]-1)*BLOCKSIZE, 0,
                                   BLOCKSIZE*(x-xstart)+BLOCKSIZE/4,
                                   BLOCKSIZE*(y-ystart)+BLOCKSIZE/4,
                                   BLOCKSIZE, BLOCKSIZE);*/
                  drawpix (cr, (block[y][x]-1)*BLOCKSIZE, 0, BLOCKSIZE*(x-xstart)+BLOCKSIZE/4, BLOCKSIZE*(y-ystart)+BLOCKSIZE/4, BLOCKSIZE, BLOCKSIZE);
            }
        }
}

void fields_attdefmsg (char *text)
{
    textbox_addtext (GTK_TEXT_VIEW(attdefwidget), text);
    adjust_bottom_text_view (GTK_TEXT_VIEW(attdefwidget));
}

void fields_attdeffmt (const char *fmt, ...)
{
    va_list ap;
    char *text = NULL;

    va_start(ap, fmt);
    text = g_strdup_vprintf(fmt,ap);
    va_end(ap);

    fields_attdefmsg (text); g_free(text);
}

void fields_attdefclear (void)
{
  gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(attdefwidget)), "", 0);
}

void fields_setlines (int l)
{
    char buf[16] = "";
    if (l >= 0)
        g_snprintf (buf, sizeof(buf), "%d", l);
    gtk_label_set_text (GTK_LABEL (lineswidget), buf);
}

void fields_setlevel (int l)
{
    char buf[16] = "";
    if (l > 0)
        g_snprintf (buf, sizeof(buf), "%d", l);
    gtk_label_set_text (GTK_LABEL (levelwidget), buf);
}

void fields_setactivelevel (int l)
{
    char buf[16] = "";
    if (l <= 0) {
        gtk_widget_set_visible (activelabel, FALSE);
        gtk_widget_set_visible (activewidget, FALSE);
    }
    else {
        g_snprintf (buf, sizeof(buf), "%d", l);
        gtk_label_set_text (GTK_LABEL (activewidget), buf);
        gtk_widget_set_visible (activelabel, TRUE);
        gtk_widget_set_visible (activewidget, TRUE);
    }
}

void fields_gmsgadd (const char *str)
{
    textbox_addtext (GTK_TEXT_VIEW(gmsgtext), str);
    adjust_bottom_text_view (GTK_TEXT_VIEW(gmsgtext));
}

void fields_gmsgclear (void)
{
  gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gmsgtext)), "", 0);
}

void fields_gmsginput (gboolean i)
{
    if (i) {
        gtk_widget_set_visible (gmsginput, TRUE);
    }
    else
        gtk_widget_set_visible (gmsginput, FALSE);
}

void fields_gmsginputclear (void)
{
    gtk_editable_set_text (GTK_EDITABLE (gmsginput), "");
    gtk_editable_set_position (GTK_EDITABLE (gmsginput), 0);
}

void fields_gmsginputactivate (int t)
{
    if (t)
    {
        fields_gmsginputclear ();
        gtk_widget_grab_focus (gmsginput);
    }
    else
        { /* do nothing */; }
}

void gmsginput_activate (void)
{
    gchar buf[512]; /* Increased from 256 to ease up for utf-8 sequences. - vidar */
    const gchar *s;

    if (gmsgstate == 0)
    {
        fields_gmsginputclear ();
        return;
    }
    s = fields_gmsginputtext ();
    if (strlen(s) > 0) {
        if (strncmp("/me ", s, 4) == 0) {
            /* post /me thingy */
            g_snprintf (buf, sizeof(buf), "* %s %s", nick, s+4);
            client_outmessage (OUT_GMSG,buf);
        }
        else {
            /* post message */
            g_snprintf (buf, sizeof(buf), "<%s> %s", nick, s);
            client_outmessage (OUT_GMSG, buf);
        }
    }
    fields_gmsginputclear ();
    fields_gmsginput (FALSE);
    unblock_keyboard_signal ();
    gmsgstate = 0;
}

const char *fields_gmsginputtext (void)
{
    return gtk_editable_get_text (GTK_EDITABLE(gmsginput));
}
