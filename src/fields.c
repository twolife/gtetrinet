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

static GtkWidget *nextpiecewidget;
    *specialwidget, *speciallabel, *attdefwidget, *lineswidget, *levelwidget,
    *activewidget, *activelabel, *gmsgtext, *gmsginput, *fieldspage, *pagecontents;
static GtkBuilder *fieldbuilders[6];

static GtkWidget *fields_page_contents (void);

static gint fields_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer field);
static gint fields_nextpiece_expose (GtkWidget *widget);
static gint fields_specials_expose (GtkWidget *widget);

static void fields_refreshfield (int field);
static void fields_drawblock (int field, int x, int y, char block);

static void gmsginput_activate (void);

static cairo_surface_t *blockpix;

static GdkColor black = {0, 0, 0, 0};
static cairo_surface_t *bitmap;
static GdkCursor *invisible_cursor, *arrow_cursor;

static FIELD displayfields[6]; /* what is actually displayed */
static TETRISBLOCK displayblock;

void fields_init (void)
{
    GtkWidget *mb;
    GdkPixbuf *pb = NULL;
    GError *err = NULL;
    cairo_surface_t *mask = NULL;
    
    if (!(pb = gdk_pixbuf_new_from_file(blocksfile, &err))) {
        mb = gtk_message_dialog_new (NULL,
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_OK,
                                     _("Error loading theme: cannot load graphics file\n"
                                       "Falling back to default"));
        gtk_dialog_run (GTK_DIALOG (mb));
        gtk_widget_destroy (mb);
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
}

void fields_cleanup (void)
{
  g_object_unref(blockpix);
}

/* a mess of functions here for creating the fields page */

GtkWidget *fields_page_new (void)
{
    pagecontents = fields_page_contents ();

    if (fieldspage == NULL) {
        fieldspage = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
        gtk_container_set_border_width (GTK_CONTAINER(fieldspage), 2);
    }
    gtk_container_add (GTK_CONTAINER(fieldspage), pagecontents);

    /* create the cursors */
    //bitmap = gdk_bitmap_create_from_data (gtk_widget_get_window(GTK_WIDGET (fieldspage)), "\0", 1, 1);
    //invisible_cursor = gdk_cursor_new_from_pixmap (bitmap, bitmap, &black, &black, 0, 0);
    //arrow_cursor = gdk_cursor_new (GDK_X_CURSOR);

    return fieldspage;
}

void fields_page_destroy_contents (void)
{
    if (pagecontents) {
        gtk_widget_destroy (pagecontents);
        pagecontents = NULL;
    }
}

GtkWidget *fields_page_contents (void)
{
    GtkBuilder *fieldsbuilder, *fieldbuilder;
    GtkWidget *some_widget;

    fieldsbuilder = gtk_builder_new_from_resource("/org/gtetrinet/fields.ui");

    /* make fields */
    {
        int playernb;
        int blocksize;
        char* playernbstr[1]; // supports up to (9+1) players ;)
        //float valign;
        GtkBuilder *fieldbuilder;
        GtkWidget *fieldparent, *fieldwidget;

        for (playernb = 0; playernb < 6; playernb ++) {
            if (playernb == 0) blocksize = BLOCKSIZE;
            else blocksize = SMALLBLOCKSIZE;
            /*
            if (playernb < 4) valign = 0.0;
            else valign = 1.0;
            */
            /* make the widgets */
            fieldbuilder = gtk_builder_new_from_resource("/org/gtetrinet/field.ui");
            fieldbuilders[playernb] = fieldbuilder;

            fields_setlabel (playernb, NULL, NULL, 0);

            fieldwidget = GTK_WIDGET(gtk_builder_get_object(fieldbuilder, "field"));
            /* attach the signals */
            g_signal_connect (G_OBJECT(fieldwidget), "draw",
                                G_CALLBACK(fields_expose_event), GINT_TO_POINTER(playernb));
            gtk_widget_set_events (fieldwidget, GDK_EXPOSURE_MASK);
            /* set the size */
            gtk_widget_set_size_request (fieldwidget,
                                         blocksize * FIELDWIDTH,
                                         blocksize * FIELDHEIGHT);

            /* align it */
            /*
            align = gtk_alignment_new (0.5, valign, 0.0, 0.0);
            gtk_container_add (GTK_CONTAINER(align), gtk_builder_get_object(fieldbuilder, "fieldparent"));
            gtk_table_attach (GTK_TABLE(table), align,
                              p[i][0], p[i][1], p[i][2], p[i][3],
                              GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
                              0, 0);
            */
            fieldparent = GTK_WIDGET(gtk_builder_get_object(fieldbuilder, "fieldparent"));
            if (playernb == 0) {
                gtk_container_add(GTK_CONTAINER(gtk_builder_get_object(fieldsbuilder, "own_field")), fieldparent);
            } else {
                g_snprintf(playernbstr, sizeof(playernbstr), "%d", playernb);
                gtk_container_add(GTK_CONTAINER(gtk_builder_get_object(fieldsbuilder, g_strconcat("field",playernbstr,NULL))), fieldparent);
            }
            /*
             * else gtk_flow_box_insert(GTK_FLOW_BOX(gtk_builder_get_object(fieldsbuilder, "other_fields")), fieldparent, -1);
             * We don't use a flow box here, because the layout for the first other field is the different to the others
             */
        }
    }

    /* next block thingy */
    nextpiecewidget = GTK_WIDGET(gtk_builder_get_object(fieldsbuilder, "next_block"));
    g_signal_connect (G_OBJECT(nextpiecewidget), "draw",
                        G_CALLBACK(fields_nextpiece_expose), NULL);
    gtk_widget_set_size_request (nextpiecewidget, BLOCKSIZE*9/2, BLOCKSIZE*9/2);

    /* lines, levels and stuff */
    activelabel = GTK_WIDGET(gtk_builder_get_object(fieldsbuilder, "activelevel_label"));
    lineswidget = GTK_WIDGET(gtk_builder_get_object(fieldsbuilder, "lines"));
    levelwidget = GTK_WIDGET(gtk_builder_get_object(fieldsbuilder, "level"));
    activewidget = GTK_WIDGET(gtk_builder_get_object(fieldsbuilder, "activelevel"));
    gtk_widget_set_size_request (GTK_WIDGET(gtk_builder_get_object(fieldsbuilder, "stuffalign")), BLOCKSIZE*6, BLOCKSIZE*11);

    /* the specials thingy */
    speciallabel = GTK_WIDGET(gtk_builder_get_object(fieldsbuilder, "specials_label"));
    specialwidget = GTK_WIDGET(gtk_builder_get_object(fieldsbuilder, "specials"));
    fields_setspeciallabel (NULL);
    g_signal_connect (G_OBJECT(specialwidget), "draw",
                        G_CALLBACK(fields_specials_expose), NULL);
    gtk_widget_set_size_request (specialwidget, BLOCKSIZE*18, BLOCKSIZE);
//    gtk_widget_set_size_request (speciallabel, BLOCKSIZE*6, -1);

    /* attacks and defenses */
    attdefwidget = GTK_WIDGET(gtk_builder_get_object(fieldsbuilder, "att_and_def"));
    gtk_text_view_set_buffer(attdefwidget, gtk_text_buffer_new(tag_table));
    gtk_widget_set_size_request (attdefwidget, MAX(22*12, BLOCKSIZE*12), BLOCKSIZE*10);

    /* game messages */
    gmsgtext = GTK_WIDGET(gtk_builder_get_object(fieldsbuilder, "game_messages"));
    gtk_text_view_set_buffer(gmsgtext, gtk_text_buffer_new(tag_table));
    gmsginput = GTK_WIDGET(gtk_builder_get_object(fieldsbuilder, "game_message_input"));
    /* eat up key messages */
    g_signal_connect (G_OBJECT(gmsginput), "activate",
                        G_CALLBACK(gmsginput_activate), NULL);

    fields_setlines (-1);
    fields_setlevel (-1);
    fields_setactivelevel (-1);
    fields_gmsginput (FALSE);

    return GTK_WIDGET(gtk_builder_get_object(fieldsbuilder, "fieldsparent"));
}


gint fields_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer field)
{
    widget = widget;
    event = event;
    fields_refreshfield (GPOINTER_TO_INT (field));
    /* hide the cursor */
    if (ingame)
      gdk_window_set_cursor (gtk_widget_get_window(widget), invisible_cursor);
    else
      gdk_window_set_cursor (gtk_widget_get_window(widget), arrow_cursor);

    return FALSE;
}

void fields_refreshfield (int field)
{
    int x, y;
    for (y = 0; y < FIELDHEIGHT; y ++)
        for (x = 0; x < FIELDWIDTH; x ++)
            fields_drawblock (field, x, y, displayfields[field][y][x]);
}

void fields_drawfield (int field, FIELD newfield)
{
    int x, y;
    for (y = 0; y < FIELDHEIGHT; y ++)
        for (x = 0; x < FIELDWIDTH; x ++)
            if (newfield[y][x] != displayfields[field][y][x]) {
                fields_drawblock (field, x, y, newfield[y][x]);
                displayfields[field][y][x] = newfield[y][x];
            }
}

void drawpix(GtkWidget *widget, int srcx, int srcy, int destx, int desty, int width, int height)
{
    cairo_t *cr = gdk_cairo_create (gtk_widget_get_window(widget));
    cairo_set_source_surface (cr, blockpix, -srcx+destx, -srcy+desty); // move big image, so the block we need is in the right position on cr
    cairo_rectangle (cr, destx, desty, width, height); // only draw the block we need
    cairo_fill(cr);
    cairo_destroy (cr);
}

void fields_drawblock (int field, int x, int y, char block)
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
    drawpix(GTK_WIDGET(gtk_builder_get_object(fieldbuilders[field], "field")), srcx, srcy, destx, desty, blocksize, blocksize);
}

void fields_setlabel (int field, char *name, char *team, int num)
{
    char buf[11];
    GtkBuilder *fieldbuilder = fieldbuilders[field];

    g_snprintf (buf, sizeof(buf), "%d", num);
    
    if (name == NULL) {
        gtk_widget_hide (GTK_LABEL(gtk_builder_get_object(fieldbuilder,"fieldnumber")));
        gtk_widget_hide (GTK_SEPARATOR(gtk_builder_get_object(fieldbuilder,"fieldnumber_separator")));
        gtk_widget_hide (GTK_LABEL(gtk_builder_get_object(fieldbuilder,"playername")));
        gtk_widget_show (GTK_LABEL(gtk_builder_get_object(fieldbuilder,"single_description")));
        gtk_widget_hide (GTK_SEPARATOR(gtk_builder_get_object(fieldbuilder,"teamname_separator")));
        gtk_widget_hide (GTK_LABEL(gtk_builder_get_object(fieldbuilder,"teamname")));
        gtk_label_set_text (GTK_LABEL(gtk_builder_get_object(fieldbuilder,"fieldnumber")), "");
        gtk_label_set_text (GTK_LABEL(gtk_builder_get_object(fieldbuilder,"playername")), "");
        gtk_label_set_text (GTK_LABEL(gtk_builder_get_object(fieldbuilder,"single_description")), _("Not playing"));
        gtk_label_set_text (GTK_LABEL(gtk_builder_get_object(fieldbuilder,"teamname")), "");
    }
    else {
        gtk_widget_show (GTK_LABEL(gtk_builder_get_object(fieldbuilders[field],"fieldnumber")));
        gtk_widget_show (GTK_SEPARATOR(gtk_builder_get_object(fieldbuilders[field],"fieldnumber_separator")));
        gtk_widget_show (GTK_LABEL(gtk_builder_get_object(fieldbuilders[field],"playername")));
        gtk_widget_hide (GTK_LABEL(gtk_builder_get_object(fieldbuilders[field],"single_description")));
        gtk_label_set_text (GTK_LABEL(gtk_builder_get_object(fieldbuilder,"fieldnumber")), buf);
        gtk_label_set_text (GTK_LABEL(gtk_builder_get_object(fieldbuilder,"playername")), name);
        gtk_label_set_text (GTK_LABEL(gtk_builder_get_object(fieldbuilder,"single_description")), "");
        if (team == NULL || team[0] == 0) {
            gtk_widget_hide (GTK_SEPARATOR(gtk_builder_get_object(fieldbuilder,"teamname_separator")));
            gtk_widget_hide (GTK_LABEL(gtk_builder_get_object(fieldbuilder,"teamname")));
            gtk_label_set_text (GTK_LABEL(gtk_builder_get_object(fieldbuilder,"teamname")), "");
        }
        else {
            gtk_widget_show (GTK_SEPARATOR(gtk_builder_get_object(fieldbuilder,"teamname_separator")));
            gtk_widget_show (GTK_LABEL(gtk_builder_get_object(fieldbuilder,"teamname")));
            gtk_label_set_text (GTK_LABEL(gtk_builder_get_object(fieldbuilder,"teamname")), team);
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

gint fields_nextpiece_expose (GtkWidget *widget)
{
    fields_drawnextblock (NULL);
    if (ingame)
      gdk_window_set_cursor (gtk_widget_get_window(widget), invisible_cursor);
    else
      gdk_window_set_cursor (gtk_widget_get_window(widget), arrow_cursor);
    return FALSE;
}

gint fields_specials_expose (GtkWidget *widget)
{
    fields_drawspecials ();
    if (ingame)
      gdk_window_set_cursor (gtk_widget_get_window(widget), invisible_cursor);
    else
      gdk_window_set_cursor (gtk_widget_get_window(widget), arrow_cursor);
    return FALSE;
}

void fields_drawspecials (void)
{
    int i;
    for (i = 0; i < 18; i ++) {
        if (i < specialblocknum) {
/*            gdk_draw_drawable (specialwidget->window,
                               specialwidget->style->black_gc,
                               blockpix, (specialblocks[i]-1)*BLOCKSIZE,
                               0, BLOCKSIZE*i, 0, BLOCKSIZE, BLOCKSIZE);*/
              drawpix (specialwidget, (specialblocks[i]-1)*BLOCKSIZE, 0, BLOCKSIZE*i, 0, BLOCKSIZE, BLOCKSIZE);
        }
        else {
/*            gdk_draw_rectangle (specialwidget->window, specialwidget->style->black_gc,
                                TRUE, BLOCKSIZE*i, 0,
                                BLOCKSIZE*(i+1), BLOCKSIZE);*/
              // black (otherwise it is white)
              cairo_t *cr = gdk_cairo_create (gtk_widget_get_window(specialwidget));
              cairo_rectangle (cr, BLOCKSIZE*i, 0, BLOCKSIZE*(i+1), BLOCKSIZE);
              cairo_fill (cr);
              cairo_destroy (cr);
        }
    }
}

void fields_drawnextblock (TETRISBLOCK block)
{
    int x, y, xstart = 4, ystart = 4, xpos, ypos;
    if (block == NULL) block = displayblock;
    // Draw the black background
    /*gdk_draw_rectangle (nextpiecewidget->window, nextpiecewidget->style->black_gc,
                        TRUE, 0, 0, BLOCKSIZE*9/2, BLOCKSIZE*9/2);*/
    cairo_t *cr = gdk_cairo_create (gtk_widget_get_window(nextpiecewidget));
    cairo_rectangle (cr, 0, 0, BLOCKSIZE*9/2, BLOCKSIZE*9/2);
    cairo_paint (cr);
    cairo_destroy (cr);
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
                  drawpix (nextpiecewidget, (block[y][x]-1)*BLOCKSIZE, 0, BLOCKSIZE*(x-xstart)+BLOCKSIZE/4, BLOCKSIZE*(y-ystart)+BLOCKSIZE/4, BLOCKSIZE, BLOCKSIZE);
            }
        }
    memcpy (displayblock, block, 16);
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
    gtk_label_set_text (lineswidget, buf);
}

void fields_setlevel (int l)
{
    char buf[16] = "";
    if (l > 0)
        g_snprintf (buf, sizeof(buf), "%d", l);
    gtk_label_set_text (levelwidget, buf);
}

void fields_setactivelevel (int l)
{
    char buf[16] = "";
    if (l <= 0) {
        gtk_widget_hide (activelabel);
        gtk_widget_hide (activewidget);
    }
    else {
        g_snprintf (buf, sizeof(buf), "%d", l);
        gtk_label_set_text (activewidget, buf);
        gtk_widget_show (activelabel);
        gtk_widget_show (activewidget);
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
        gtk_widget_show (gmsginput);
    }
    else
        gtk_widget_hide (gmsginput);
}

void fields_gmsginputclear (void)
{
    gtk_entry_set_text (GTK_ENTRY (gmsginput), "");
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
    return gtk_entry_get_text (GTK_ENTRY(gmsginput));
}
