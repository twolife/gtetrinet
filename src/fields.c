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
#include <gdk_imlib.h>
#include <stdio.h>

#include "config.h"
#include "client.h"
#include "tetrinet.h"
#include "tetris.h"
#include "fields.h"
#include "misc.h"

#define BLOCKSIZE bsize
#define SMALLBLOCKSIZE (BLOCKSIZE/2)

static GtkWidget *fieldwidgets[6], *nextpiecewidget, *fieldlabels[6][6],
    *specialwidget, *speciallabel, *attdefwidget, *lineswidget, *levelwidget,
    *activewidget, *activelabel, *gmsgtext, *gmsginput, *fieldspage, *pagecontents;

static GtkWidget *fields_page_contents (void);

static gint fields_expose_event (GtkWidget *widget, GdkEventExpose *event, int field);
static gint fields_nextpiece_expose (GtkWidget *widget, GdkEventExpose *event);
static gint fields_specials_expose (GtkWidget *widget, GdkEventExpose *event);

static void fields_refreshfield (int field);
static void fields_drawblock (int field, int x, int y, char block);

static gint fields_eatkey (GtkWidget *widget, GdkEventKey *key);

static GdkPixmap *blockpix;

static FIELD displayfields[6]; /* what is actually displayed */
static TETRISBLOCK displayblock;

void fields_init (void)
{
    GdkBitmap *bmap;
    GtkWidget *mb;
    if (gdk_imlib_load_file_to_pixmap (blocksfile, &blockpix, &bmap) == 0) {
        mb = gnome_message_box_new (_("Error loading theme: cannot load graphics file\n"
                                      "Falling back to default"),
                                    GNOME_MESSAGE_BOX_ERROR, GNOME_STOCK_BUTTON_OK, NULL);
        gnome_dialog_run (GNOME_DIALOG(mb));
        config_loadtheme (DEFAULTTHEME);
        if (gdk_imlib_load_file_to_pixmap (blocksfile, &blockpix, &bmap) == 0) {
            /* shouldnt happen */
            fprintf (stderr, _("Error loading default theme: Aborting...\n"
                               "Check for installation errors\n"));
            exit (0);
        }
    }
    /* we dont want the bitmap mask */
}

void fields_cleanup (void)
{
    gdk_imlib_free_pixmap (blockpix);
}

/* a mess of functions here for creating the fields page */

GtkWidget *fields_page_new (void)
{
    pagecontents = fields_page_contents ();

    if (fieldspage == NULL) {
        fieldspage = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
        gtk_container_border_width (GTK_CONTAINER(fieldspage), 2);
    }
    gtk_container_add (GTK_CONTAINER(fieldspage), pagecontents);

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
    GtkWidget *table, *widget, *align, *border, *box, *table2, *hbox, *scroll;
    table = gtk_table_new (4, 5, FALSE);

    gtk_table_set_row_spacings (GTK_TABLE(table), 2);
    gtk_table_set_col_spacings (GTK_TABLE(table), 2);

    /* make fields */
    {
        int p[6][4] = { /* table attach positions */
            {0, 1, 0, 2},
            {2, 3, 0, 1},
            {3, 4, 0, 1},
            {4, 5, 0, 1},
            {3, 4, 1, 3},
            {4, 5, 1, 3}
        };
        int i;
        int blocksize;
        float valign;
        for (i = 0; i < 6; i ++) {
            if (i == 0) blocksize = BLOCKSIZE;
            else blocksize = SMALLBLOCKSIZE;
            if (i < 4) valign = 0.0;
            else valign = 1.0;
            /* make the widgets */
            box = gtk_vbox_new (FALSE, 0);
            /* the labels */
            fieldlabels[i][0] = gtk_label_new ("");
            fieldlabels[i][1] = gtk_vseparator_new ();
            fieldlabels[i][2] = gtk_label_new ("");
            fieldlabels[i][3] = gtk_label_new ("");
            fieldlabels[i][4] = gtk_vseparator_new ();
            fieldlabels[i][5] = gtk_label_new ("");

            hbox = gtk_hbox_new (FALSE, 0);
            gtk_box_pack_start (GTK_BOX(hbox), fieldlabels[i][0], FALSE, FALSE, 2);
            gtk_box_pack_start (GTK_BOX(hbox), fieldlabels[i][1], FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX(hbox), fieldlabels[i][2], FALSE, FALSE, 2);
            gtk_box_pack_start (GTK_BOX(hbox), fieldlabels[i][3], TRUE, TRUE, 0);
            gtk_box_pack_start (GTK_BOX(hbox), fieldlabels[i][4], FALSE, FALSE, 2);
            gtk_box_pack_start (GTK_BOX(hbox), fieldlabels[i][5], FALSE, FALSE, 0);
            gtk_widget_show (hbox);

            fields_setlabel (i, NULL, NULL, 0);

            widget = gtk_event_box_new ();
            gtk_container_add (GTK_CONTAINER(widget), hbox);
            gtk_widget_set_usize (widget, blocksize * FIELDWIDTH, 0);
            gtk_widget_show (widget);
            gtk_box_pack_start (GTK_BOX(box), widget, FALSE, FALSE, 0);
            /* the field */
            fieldwidgets[i] = gtk_drawing_area_new ();
            /* attach the signals */
            gtk_signal_connect (GTK_OBJECT(fieldwidgets[i]), "expose_event",
                                GTK_SIGNAL_FUNC(fields_expose_event), (gpointer)i);
            gtk_widget_set_events (fieldwidgets[i], GDK_EXPOSURE_MASK);
            /* set the size */
            gtk_drawing_area_size (GTK_DRAWING_AREA(fieldwidgets[i]),
                                   blocksize * FIELDWIDTH,
                                   blocksize * FIELDHEIGHT);
            gtk_widget_show (fieldwidgets[i]);
            gtk_box_pack_start (GTK_BOX(box), fieldwidgets[i], FALSE, FALSE, 0);
            gtk_widget_show (box);
            border = gtk_frame_new (NULL);
            gtk_frame_set_shadow_type (GTK_FRAME(border), GTK_SHADOW_IN);
            gtk_container_add (GTK_CONTAINER(border), box);
            gtk_widget_show (border);
            /* align it */
            align = gtk_alignment_new (0.5, valign, 0.0, 0.0);
            gtk_container_add (GTK_CONTAINER(align), border);
            gtk_widget_show (align);
            gtk_table_attach_defaults (GTK_TABLE(table), align,
                                       p[i][0], p[i][1], p[i][2], p[i][3]);
        }
    }
    /* next block thingy */
    box = gtk_vbox_new (FALSE, 2);

    widget = leftlabel_new (_("Next piece:"));
    gtk_widget_show (widget);
    gtk_box_pack_start (GTK_BOX(box), widget, FALSE, FALSE, 0);
    /* box that displays the next block */
    border = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(border), GTK_SHADOW_IN);
    nextpiecewidget = gtk_drawing_area_new ();
    gtk_signal_connect (GTK_OBJECT(nextpiecewidget), "expose_event",
                        GTK_SIGNAL_FUNC(fields_nextpiece_expose), NULL);
    gtk_widget_set_events (nextpiecewidget, GDK_EXPOSURE_MASK);
    gtk_drawing_area_size (GTK_DRAWING_AREA(nextpiecewidget), BLOCKSIZE*9/2, BLOCKSIZE*9/2);
    gtk_widget_show (nextpiecewidget);
    gtk_container_add (GTK_CONTAINER(border), nextpiecewidget);
    gtk_widget_show (border);
    align = gtk_alignment_new (0.5, 0.5, 0, 0);
    gtk_container_add (GTK_CONTAINER(align), border);
    gtk_widget_show (align);
    gtk_box_pack_start (GTK_BOX(box), align, FALSE, FALSE, 0);
    /* lines, levels and stuff */
    table2 = gtk_table_new (4, 2, FALSE);
    gtk_table_set_col_spacings (GTK_TABLE(table2), 5);
    widget = leftlabel_new (_("Lines:"));
    gtk_widget_show (widget);
    gtk_table_attach_defaults (GTK_TABLE(table2), widget, 0, 1, 0, 1);
    widget = gtk_label_new ("");
    gtk_widget_show (widget);
    gtk_table_attach_defaults (GTK_TABLE(table2), widget, 0, 1, 1, 2);
    widget = leftlabel_new (_("Level:"));
    gtk_widget_show (widget);
    gtk_table_attach_defaults (GTK_TABLE(table2), widget, 0, 1, 2, 3);
    activelabel = leftlabel_new (_("Active level:"));
    gtk_widget_show (activelabel);
    gtk_table_attach_defaults (GTK_TABLE(table2), activelabel, 0, 1, 3, 4);
    lineswidget = leftlabel_new ("");
    gtk_widget_show (lineswidget);
    gtk_table_attach_defaults (GTK_TABLE(table2), lineswidget, 1, 2, 0, 1);
    widget = gtk_label_new ("");
    gtk_widget_show (widget);
    gtk_table_attach_defaults (GTK_TABLE(table2), widget, 1, 2, 1, 2);
    levelwidget = leftlabel_new ("");
    gtk_widget_show (levelwidget);
    gtk_table_attach_defaults (GTK_TABLE(table2), levelwidget, 1, 2, 2, 3);
    activewidget = leftlabel_new ("");
    gtk_widget_show (activewidget);
    gtk_table_attach_defaults (GTK_TABLE(table2), activewidget, 1, 2, 3, 4);
    gtk_widget_show (table2);
    gtk_box_pack_start (GTK_BOX(box), table2, FALSE, FALSE, 0);

    gtk_widget_show (box);
    /* align it */
    align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
    gtk_widget_set_usize (align, BLOCKSIZE*6, BLOCKSIZE*11);
    gtk_container_add (GTK_CONTAINER(align), box);
    gtk_widget_show (align);
    gtk_table_attach_defaults (GTK_TABLE(table), align, 1, 2, 0, 1);

    /* the specials thingy */
    box = gtk_hbox_new (FALSE, 0);
    speciallabel = gtk_label_new ("");
    gtk_widget_show (speciallabel);
    fields_setspeciallabel (NULL);
    gtk_box_pack_start (GTK_BOX(box), speciallabel, FALSE, FALSE, 0);
    border = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(border), GTK_SHADOW_IN);
    specialwidget = gtk_drawing_area_new ();
    gtk_signal_connect (GTK_OBJECT(specialwidget), "expose_event",
                        GTK_SIGNAL_FUNC(fields_specials_expose), NULL);
    gtk_drawing_area_size (GTK_DRAWING_AREA(specialwidget), BLOCKSIZE*18, BLOCKSIZE);
    gtk_widget_show (specialwidget);
    gtk_container_add (GTK_CONTAINER(border), specialwidget);
    gtk_widget_show (border);
    gtk_box_pack_end (GTK_BOX(box), border, FALSE, FALSE, 0);
    gtk_widget_show (box);
    gtk_widget_set_usize (box, BLOCKSIZE*24, 0);
    /* align it */
    align = gtk_alignment_new (0.5, 1.0, 0.7, 0.0);
    gtk_container_add (GTK_CONTAINER(align), box);
    gtk_widget_show (align);
    gtk_table_attach_defaults (GTK_TABLE (table), align, 0, 3, 2, 3);

    /* attacks and defenses */

    box = gtk_vbox_new (FALSE, 0);
    widget = gtk_label_new (_("Attacks and defenses:"));
    gtk_widget_show (widget);
    gtk_box_pack_start (GTK_BOX(box), widget, FALSE, FALSE, 0);
    attdefwidget = gtk_text_new (NULL, NULL);
    gtk_widget_set_usize (attdefwidget, BLOCKSIZE*12, BLOCKSIZE*10);
    gtk_text_set_word_wrap (GTK_TEXT(attdefwidget), TRUE);
    GTK_WIDGET_UNSET_FLAGS (attdefwidget, GTK_CAN_FOCUS);
    gtk_widget_show (attdefwidget);
    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER(scroll), attdefwidget);
    gtk_widget_show (scroll);
    gtk_box_pack_start (GTK_BOX(box), scroll, FALSE, FALSE, 0);
    gtk_widget_show (box);
    align = gtk_alignment_new (0.5, 0.5, 0.5, 0.0);
    gtk_container_add (GTK_CONTAINER(align), box);
    gtk_widget_show (align);
    gtk_table_attach_defaults (GTK_TABLE(table), align, 1, 3, 1, 2);

    /* game messages */
    table2 = gtk_table_new (1, 2, FALSE);
    gmsgtext = gtk_text_new (NULL, NULL);
    gtk_widget_set_usize (gmsgtext, 0, 46);
    gtk_widget_show (gmsgtext);
    GTK_WIDGET_UNSET_FLAGS (gmsgtext, GTK_CAN_FOCUS);
    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER(scroll), gmsgtext);
    gtk_widget_show (scroll);
    gtk_table_attach (GTK_TABLE(table2), scroll, 0, 1, 0, 1,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_SHRINK,
                      0, 0);
    gmsginput = gtk_entry_new_with_max_length (128);
    gtk_widget_show (gmsginput);
    /* eat up key messages */
    gtk_signal_connect (GTK_OBJECT(gmsginput), "key_press_event",
                        GTK_SIGNAL_FUNC(fields_eatkey), NULL);
    gtk_table_attach (GTK_TABLE(table2), gmsginput, 0, 1, 1, 2,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);
    gtk_widget_show (table2);
    gtk_widget_set_usize (table2, 0, 48);
    gtk_table_attach_defaults (GTK_TABLE(table), table2, 0, 5, 3, 4);

    gtk_widget_show (table);

    fields_setlines (-1);
    fields_setlevel (-1);
    fields_setactivelevel (-1);
    fields_gmsginput (FALSE);

    return table;
}


gint fields_expose_event (GtkWidget *widget, GdkEventExpose *event, int field)
{
    fields_refreshfield (field);

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

    gdk_draw_pixmap (fieldwidgets[field]->window,
                     fieldwidgets[field]->style->black_gc,
                     blockpix, srcx, srcy, destx, desty,
                     blocksize, blocksize);
}

void fields_setlabel (int field, char *name, char *team, int num)
{
    char buf[10];

    sprintf (buf, "%d", num);
    
    if (name == NULL) {
        gtk_widget_hide (fieldlabels[field][0]);
        gtk_widget_hide (fieldlabels[field][1]);
        gtk_widget_hide (fieldlabels[field][2]);
        gtk_widget_show (fieldlabels[field][3]);
        gtk_widget_hide (fieldlabels[field][4]);
        gtk_widget_hide (fieldlabels[field][5]);
        gtk_label_set (GTK_LABEL(fieldlabels[field][0]), "");
        gtk_label_set (GTK_LABEL(fieldlabels[field][2]), "");
        gtk_label_set (GTK_LABEL(fieldlabels[field][3]), _("Not playing"));
        gtk_label_set (GTK_LABEL(fieldlabels[field][5]), "");
    }
    else {
        gtk_widget_show (fieldlabels[field][0]);
        gtk_widget_show (fieldlabels[field][1]);
        gtk_widget_show (fieldlabels[field][2]);
        gtk_widget_hide (fieldlabels[field][3]);
        gtk_label_set (GTK_LABEL(fieldlabels[field][0]), buf);
        gtk_label_set (GTK_LABEL(fieldlabels[field][2]), nocolor(name));
        gtk_label_set (GTK_LABEL(fieldlabels[field][3]), "");
        if (team == NULL || team[0] == 0) {
            gtk_widget_hide (fieldlabels[field][4]);
            gtk_widget_hide (fieldlabels[field][5]);
            gtk_label_set (GTK_LABEL(fieldlabels[field][5]), "");
        }
        else {
            gtk_widget_show (fieldlabels[field][4]);
            gtk_widget_show (fieldlabels[field][5]);
            gtk_label_set (GTK_LABEL(fieldlabels[field][5]), nocolor(team));
        }
    }
}

void fields_setspeciallabel (char *label)
{
    if (label == NULL) {
        gtk_label_set (GTK_LABEL(speciallabel), _("Specials:"));
    }
    else {
        gtk_label_set (GTK_LABEL(speciallabel), label);
    }
}

gint fields_nextpiece_expose (GtkWidget *widget, GdkEventExpose *event)
{
    fields_drawnextblock (NULL);
    return FALSE;
}

gint fields_specials_expose (GtkWidget *widget, GdkEventExpose *event)
{
    fields_drawspecials ();
    return FALSE;
}

void fields_drawspecials (void)
{
    int i;
    for (i = 0; i < 18; i ++) {
        if (i < specialblocknum) {
            gdk_draw_pixmap (specialwidget->window,
                             specialwidget->style->black_gc,
                             blockpix, (specialblocks[i]-1)*BLOCKSIZE,
                             0, BLOCKSIZE*i, 0, BLOCKSIZE, BLOCKSIZE);
        }
        else {
            gdk_draw_rectangle (specialwidget->window, specialwidget->style->black_gc,
                                TRUE, BLOCKSIZE*i, 0,
                                BLOCKSIZE*(i+1), BLOCKSIZE);
        }
    }
}

void fields_drawnextblock (TETRISBLOCK block)
{
    int x, y, xstart = 4, ystart = 4;
    if (block == NULL) block = displayblock;
    gdk_draw_rectangle (nextpiecewidget->window, nextpiecewidget->style->black_gc,
                        TRUE, 0, 0, BLOCKSIZE*9/2, BLOCKSIZE*9/2);
    for (y = 0; y < 4; y ++)
        for (x = 0; x < 4; x ++)
            if (block[y][x]) {
                if (y < ystart) ystart = y;
                if (x < xstart) xstart = x;
            }
    for (y = ystart; y < 4; y ++)
        for (x = xstart; x < 4; x ++) {
            if (block[y][x]) {
                gdk_draw_pixmap (nextpiecewidget->window,
                                 nextpiecewidget->style->black_gc,
                                 blockpix, (block[y][x]-1)*BLOCKSIZE, 0,
                                 BLOCKSIZE*(x-xstart)+BLOCKSIZE/4,
                                 BLOCKSIZE*(y-ystart)+BLOCKSIZE/4,
                                 BLOCKSIZE, BLOCKSIZE);
            }
        }
    memcpy (displayblock, block, 16);
}

void fields_attdefmsg (char *text)
{
    textbox_addtext (GTK_TEXT(attdefwidget), text);
    adjust_bottom (GTK_TEXT(attdefwidget)->vadj);
}

void fields_attdefclear (void)
{
    gtk_text_freeze (GTK_TEXT(attdefwidget));
    gtk_text_backward_delete (GTK_TEXT(attdefwidget),
                              gtk_text_get_point (GTK_TEXT(attdefwidget)));
    gtk_text_thaw (GTK_TEXT(attdefwidget));
}

void fields_setlines (int l)
{
    char buf[16] = "";
    if (l >= 0)
        sprintf (buf, "%d", l);
    leftlabel_set (lineswidget, buf);
}

void fields_setlevel (int l)
{
    char buf[16] = "";
    if (l > 0)
        sprintf (buf, "%d", l);
    leftlabel_set (levelwidget, buf);
}

void fields_setactivelevel (int l)
{
    char buf[16] = "";
    if (l <= 0) {
        gtk_widget_hide (activelabel);
        gtk_widget_hide (activewidget);
    }
    else {
        sprintf (buf, "%d", l);
        leftlabel_set (activewidget, buf);
        gtk_widget_show (activelabel);
        gtk_widget_show (activewidget);
    }
}

void fields_gmsgadd (char *str)
{
    textbox_addtext (GTK_TEXT(gmsgtext), str);
    adjust_bottom (GTK_TEXT(gmsgtext)->vadj);
}

void fields_gmsgclear (void)
{
    gtk_text_freeze (GTK_TEXT(gmsgtext));
    gtk_text_backward_delete (GTK_TEXT(gmsgtext),
                              gtk_text_get_point (GTK_TEXT(gmsgtext)));
    gtk_text_thaw (GTK_TEXT(gmsgtext));
}

void fields_gmsginput (int i)
{
    if (i) {
        gtk_widget_show (gmsginput);
    }
    else
        gtk_widget_hide (gmsginput);
}

void fields_gmsginputclear (void)
{
    gtk_entry_set_text (GTK_ENTRY(gmsginput), "");
    gtk_entry_set_position (GTK_ENTRY(gmsginput), 0);
}

void fields_gmsginputactivate (int t)
{
    if (t)
        gtk_widget_grab_focus (gmsginput);
    else
        /* do nothing */;
}

void fields_gmsginputadd (char *c)
{
    gtk_entry_append_text (GTK_ENTRY(gmsginput), c);
    gtk_entry_set_position (GTK_ENTRY(gmsginput),
                            strlen(gtk_entry_get_text(GTK_ENTRY(gmsginput))));
}

void fields_gmsginputback (void)
{
    char buf[256];
    strcpy (buf, gtk_entry_get_text(GTK_ENTRY(gmsginput)));
    if (strlen(buf) == 0) return;
    buf[strlen(buf)-1] = 0;
    gtk_entry_set_text (GTK_ENTRY(gmsginput), buf);
    gtk_entry_set_position (GTK_ENTRY(gmsginput), strlen(buf));
}

char *fields_gmsginputtext (void)
{
    return gtk_entry_get_text (GTK_ENTRY(gmsginput));
}

gint fields_eatkey (GtkWidget *widget, GdkEventKey *key)
{
    gtk_signal_emit_stop_by_name (GTK_OBJECT(widget), "key_press_event");
    return TRUE;
}
