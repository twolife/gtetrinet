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
#include <unistd.h>
#include <stdlib.h>

#include "misc.h"

/* a left aligned label */
GtkWidget *leftlabel_new (char *str)
{
    GtkWidget *label, *align;
    label = gtk_label_new (str);
    gtk_label_set_justify (GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap (GTK_LABEL(label), TRUE);
    gtk_widget_show (label);
    align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
    gtk_container_add (GTK_CONTAINER(align), label);
    return align;
}

void leftlabel_set (GtkWidget *align, char *str)
{
    gtk_label_set (GTK_LABEL(GTK_BIN(align)->child), str);
}

/* returns a random number in the range 0 to n-1 */
int randomnum (int n)
{
    return (float)n*random()/(RAND_MAX+1.0);
}

void fdreadline (int fd, char *buf)
{
    int c = 0;
    /* read a single line - no overflow check */
    while (1) {
        if (read (fd, &buf[c], 1) == 0) break;
        if (buf[c] == '\n') break;
        c ++;
    }
    buf[c] = 0;
}

#define COLORNUM 26

static GdkColor colors[] =
{
    {0, 0, 0, 0},                /* ^A */
    {0, 0, 0, 0},                /* ^B */
    {0, 0x0000, 0xFFFF, 0xFFFF}, /* ^C cyan */
    {0, 0x0000, 0x0000, 0x0000}, /* ^D black */
    {0, 0x0000, 0x0000, 0xFFFF}, /* ^E bright blue */
    {0, 0x7FFF, 0x7FFF, 0x7FFF}, /* ^F grey */
    {0, 0, 0, 0},                /* ^G */
    {0, 0xFFFF, 0x0000, 0xFFFF}, /* ^H magenta */
    {0, 0, 0, 0},                /* ^I */
    {0, 0, 0, 0},                /* ^J */
    {0, 0x7FFF, 0x7FFF, 0x7FFF}, /* ^K grey */
    {0, 0x0000, 0x7FFF, 0x0000}, /* ^L dark green */
    {0, 0, 0, 0},                /* ^M */
    {0, 0x0000, 0xFFFF, 0x0000}, /* ^N bright green */
    {0, 0xBFFF, 0xBFFF, 0xBFFF}, /* ^O light grey */
    {0, 0x7FFF, 0x0000, 0x0000}, /* ^P dark red */
    {0, 0x0000, 0x0000, 0x7FFF}, /* ^Q dark blue */
    {0, 0x7FFF, 0x7FFF, 0x0000}, /* ^R brown */
    {0, 0x7FFF, 0x0000, 0x7FFF}, /* ^S purple */
    {0, 0xFFFF, 0x0000, 0x0000}, /* ^T bright red */
    {0, 0xBFFF, 0xBFFF, 0xBFFF}, /* ^U light grey */
    {0, 0, 0, 0},                /* ^V */
    {0, 0x0000, 0x7FFF, 0x7FFF}, /* ^W dark cyan */
    {0, 0xFFFF, 0xFFFF, 0xFFFF}, /* ^X white */
    {0, 0xFFFF, 0xFFFF, 0x0000}, /* ^Y yellow */
    {0, 0, 0, 0}                 /* ^Z */
};

static GdkFont *fonts[4];

void textbox_setup (void)
{
    int n;
    GdkColormap *colormap = gdk_colormap_get_system ();
    for (n = 0; n < COLORNUM; n ++)
        gdk_color_alloc (colormap, &colors[n]);
    /* hard coded fonts ?!?!?!? */
    /* normal */
    fonts[0] = gdk_fontset_load("-adobe-helvetica-medium-r-normal--*-120-*-*-*-*-*-*,*--12-*");
    /* bold */
    fonts[1] = gdk_fontset_load("-adobe-helvetica-bold-r-normal--*-120-*-*-*-*-*-*,*--12-*");
    /* italic */
    fonts[2] = gdk_fontset_load("-adobe-helvetica-medium-o-normal--*-120-*-*-*-*-*-*,*--12-*");
    /* bold italic */
    fonts[3] = gdk_fontset_load("-adobe-helvetica-bold-o-normal--*-120-*-*-*-*-*-*,*--12-*");
}

void textbox_addtext (GtkText *textbox, unsigned char *text)
{
    GdkColor *color, *lastcolor;
    int attr; /* bits are used as flags: bold, italic */
    int i, bottom;
    char last;
    GtkAdjustment *textboxadj = GTK_TEXT(textbox)->vadj;

    /* is the scroll bar at the bottom ?? */
    if ((textboxadj->value+10)>(textboxadj->upper-textboxadj->lower-textboxadj->page_size))
        bottom = TRUE;
    else bottom = FALSE;

    lastcolor = color = &GTK_WIDGET(textbox)->style->black;
    last = 0; attr = 0;
    gtk_text_freeze (textbox);

    if (gtk_text_get_length (textbox)) /* not first line */
        gtk_text_insert (textbox, fonts[attr], color, NULL, "\n", 1);
    for (i = 0; text[i]; i ++) {
	if (text[i] == 0xFF) { /* reset */
	    lastcolor = color = &GTK_WIDGET(textbox)->style->black;
	    last = 0; attr = 0;
	}
        else if (text[i] < 32) {
            switch (text[i]) {
            case 0x02: attr = attr ^ 0x01; break; /* bold */
            case 0x16: attr = attr ^ 0x02; break; /* italics */
            case 0x1F: break; /* underline not available */
            default: /* it is a color... */
                if (text[i] > 0x1A) goto next; /* bounds checking */
                if (text[i] == last) {
                    /* restore previous color */
                    color = lastcolor;
                    last = 0;
                    goto next;
                }
                /* save color */
                lastcolor = color;
                last = text[i];
                /* get new color */
                color = &colors[text[i]-0x01];
            }
        }
        else gtk_text_insert (textbox, fonts[attr], color, NULL, &text[i], 1);
    next:
    }
    /* scroll to bottom */
    gtk_text_thaw (textbox);
    if (bottom) adjust_bottom (textboxadj);
}

void adjust_bottom (GtkAdjustment *adj)
{
    gtk_adjustment_set_value (adj, adj->upper - adj->lower - adj->page_size);
}

char *nocolor (char *str)
{
    static char buf[1024], *p;
    for (p = buf; *str; str ++) {
        if (*str > 0x1F) *p++ = *str;
    }
    *p = 0;
    return buf;
}

GtkWidget *pixmap_label (GdkPixmap *pm, GdkBitmap *mask, char *str)
{
    GtkWidget *box, *widget;
    box = gtk_hbox_new (FALSE, 0);
    widget = gtk_pixmap_new (pm, mask);
    gtk_widget_show (widget);
    gtk_box_pack_start (GTK_BOX(box), widget, TRUE, TRUE, 0);
    widget = gtk_label_new (str);
    gtk_widget_show (widget);
    gtk_box_pack_start (GTK_BOX(box), widget, TRUE, TRUE, 0);
    return box;
}
