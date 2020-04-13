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

#include <gtk/gtk.h>
#include <unistd.h>
#include <stdlib.h>

#include "misc.h"
#include "gtetrinet.h"
#include "string.h"


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
    gtk_label_set_text (GTK_LABEL(gtk_bin_get_child(GTK_BIN(align))), str);
}

/* returns a random number in the range 0 to n-1 --
 * Note both n==0 and n==1 always return 0 */
int randomnum (int n)
{
    return (float)n*rand()/(RAND_MAX+1.0);
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

static struct gtet_text_tags {
 GdkColor c;
 GtkTextTag *t_c;
} gtet_text_tags[] =
{
                                         /* Code + 0xE000 */
    {{0, 0, 0, 0}, NULL},                /* ^A black */
    {{0, 0, 0, 0}, NULL},                /* ^B black */
    {{0, 0x0000, 0xFFFF, 0xFFFF}, NULL}, /* ^C cyan */
    {{0, 0x0000, 0x0000, 0x0000}, NULL}, /* ^D black */
    {{0, 0x0000, 0x0000, 0xFFFF}, NULL}, /* ^E bright blue */
    {{0, 0x7FFF, 0x7FFF, 0x7FFF}, NULL}, /* ^F grey */
    {{0, 0, 0, 0}, NULL},                /* ^G black */
    {{0, 0xFFFF, 0x0000, 0xFFFF}, NULL}, /* ^H magenta */
    {{0, 0, 0, 0}, NULL},                /* ^I black */
    {{0, 0, 0, 0}, NULL},                /* ^J black */
    {{0, 0x7FFF, 0x7FFF, 0x7FFF}, NULL}, /* ^K grey */
    {{0, 0x0000, 0x7FFF, 0x0000}, NULL}, /* ^L dark green */
    {{0, 0, 0, 0}, NULL},                /* ^M black */
    {{0, 0x0000, 0xFFFF, 0x0000}, NULL}, /* ^N bright green */
    {{0, 0xBFFF, 0xBFFF, 0xBFFF}, NULL}, /* ^O light grey */
    {{0, 0x7FFF, 0x0000, 0x0000}, NULL}, /* ^P dark red */
    {{0, 0x0000, 0x0000, 0x7FFF}, NULL}, /* ^Q dark blue */
    {{0, 0x7FFF, 0x7FFF, 0x0000}, NULL}, /* ^R brown */
    {{0, 0x7FFF, 0x0000, 0x7FFF}, NULL}, /* ^S purple */
    {{0, 0xFFFF, 0x0000, 0x0000}, NULL}, /* ^T bright red */
    {{0, 0xBFFF, 0xBFFF, 0xBFFF}, NULL}, /* ^U light grey */
    {{0, 0, 0, 0}, NULL},                /* ^V black */
    {{0, 0x0000, 0x7FFF, 0x7FFF}, NULL}, /* ^W dark cyan */
    {{0, 0xFFFF, 0xFFFF, 0xFFFF}, NULL}, /* ^X white */
    {{0, 0xFFFF, 0xFFFF, 0x0000}, NULL}, /* ^Y yellow */
    {{0, 0, 0, 0}, NULL}                 /* ^Z black */
};

static GtkTextTag *t_bold = NULL;
static GtkTextTag *t_italic = NULL;
static GtkTextTag *t_underline = NULL;

GtkTextTagTable *tag_table = NULL;

void textbox_setup (void)
{ /* API is shafted, so we need to create a buffer,
   * to use the nice API for adding tags to tag tables ... then we destroy the
   * buffer and keep the table for use with new buffers */
    int n;
    GtkTextBuffer *buffer = gtk_text_buffer_new(NULL);
    
    for (n = 0; n < COLORNUM; n ++)
        gtet_text_tags[n].t_c = gtk_text_buffer_create_tag (buffer, NULL,
                                                            "foreground-gdk",
                                                            &gtet_text_tags[n].c,
                                                            NULL);

    t_bold = gtk_text_buffer_create_tag (buffer, NULL,
                                         "weight", PANGO_WEIGHT_BOLD, NULL);
    t_italic = gtk_text_buffer_create_tag (buffer, NULL,
                                           "style", PANGO_STYLE_ITALIC, NULL);
    t_underline = gtk_text_buffer_create_tag (buffer, NULL,
                                              "underline", PANGO_UNDERLINE_SINGLE, NULL);

    tag_table = gtk_text_buffer_get_tag_table(buffer);
}

void textbox_addtext (GtkTextView *textbox, const char *str)
{
    /* At this point, str should be valid utf-8 except for some 0xFF-bytes for
     * formatting. */

    GtkTextTag *color, *lastcolor;
    /* int bottom; */
    gunichar last = 0;
    gunichar tmp;
    gboolean attr_bold = FALSE;
    gboolean attr_italic = FALSE;
    gboolean attr_underline = FALSE;
    /* GtkAdjustment *textboxadj = GTK_TEXT_VIEW(textbox)->vadjustment; */
    GtkTextIter iter;
    gchar* p;
    gchar* text=g_strdup(str);

                
    /* is the scroll bar at the bottom ?? */
    /* if ((textboxadj->value+10)>(textboxadj->upper-textboxadj->lower-textboxadj->page_size))
       bottom = TRUE;
       else bottom = FALSE; */
    /* FIXME: this shouldn't be here ... GtkTextView craps itself if you adjust
     * constantly ...
     * plus this function is only used from one place that doesn't call
     * adjust_bottom always anyway  */
    
    lastcolor = color = gtet_text_tags[0].t_c;
    /* gtk_text_freeze (textbox); */

    gtk_text_buffer_get_end_iter(gtk_text_view_get_buffer(textbox), &iter);
    
    if (gtk_text_buffer_get_char_count (gtk_text_view_get_buffer(textbox))) /* not first line */
        gtk_text_buffer_insert (gtk_text_view_get_buffer(textbox), &iter, "\n", 1);

    /* For-loop with utf-8 support. - vidar*/
    /* XXX: Relies on g_utf8_next_char advancing by one byte on an invalid start byte,
       which is undefined by the documentation of g_utf8_next_char. -stump */
    for(p=text; p[0]; p=g_utf8_next_char(p)) {
        tmp=(guchar)p[0];  /* Only for checking color codes now.*/
        
	if (tmp == TETRI_TB_RESET) {
	    lastcolor = color = gtet_text_tags[0].t_c;
            attr_bold = FALSE;
            attr_italic = FALSE;
            attr_underline = FALSE;
	}
        else if (tmp <= TETRI_TB_END_OFFSET) {
            g_assert(TETRI_TB_END_OFFSET < 32); /* ASCII space */
            g_assert(TETRI_TB_C_END_OFFSET <= TETRI_TB_END_OFFSET);
          
            switch (tmp) {
            case TETRI_TB_BOLD:      attr_bold      = !attr_bold; break;
            case TETRI_TB_ITALIC:    attr_italic    = !attr_italic; break;
            case TETRI_TB_UNDERLINE: attr_underline = !attr_underline; break;
            default: /* it is a color... */
                if (tmp > TETRI_TB_C_END_OFFSET) break;
                if (tmp == last) {
                    /* restore previous color */
                    color = lastcolor;
                    last = 0;
                    goto next;
                }
                /* save color */
                lastcolor = color;
                last = tmp;
                /* get new color */
                color = gtet_text_tags[tmp - TETRI_TB_C_BEG_OFFSET].t_c;
            }
        }
        else
        {
          tmp=g_utf8_get_char(p); /* It's not a color code, so get the entire char. */
          /* gchar *out = g_locale_to_utf8 (&text[i], 1, NULL, NULL, NULL);  */
          gchar out[7]; /* max utf-8 length plus \0 */
          out[g_unichar_to_utf8(tmp,out)]='\0'; /* convert and terminate */
          g_assert(g_utf8_validate(out,-1,NULL));
          
            if (0)
            { /* do nothing */ ; }
            else if (attr_bold && attr_italic && attr_underline)
              gtk_text_buffer_insert_with_tags (gtk_text_view_get_buffer(textbox), &iter, out, -1,
                                                t_bold, t_italic, t_underline,
                                                color, NULL);
            else if (attr_bold && attr_italic)
              gtk_text_buffer_insert_with_tags (gtk_text_view_get_buffer(textbox), &iter, out, -1,
                                                t_bold, t_italic,
                                                color, NULL);
            else if (attr_bold && attr_underline)
              gtk_text_buffer_insert_with_tags (gtk_text_view_get_buffer(textbox), &iter, out, -1,
                                                t_bold, t_underline,
                                                color, NULL);
            else if (attr_italic && attr_underline)
              gtk_text_buffer_insert_with_tags (gtk_text_view_get_buffer(textbox), &iter, out, -1,
                                                t_italic, t_underline,
                                                color, NULL);
            else if (attr_bold || attr_italic || attr_underline)
            {
              GtkTextTag *t_a = NULL;

              if (attr_bold)      t_a = t_bold;
              if (attr_italic)    t_a = t_italic;
              if (attr_underline) t_a = t_underline;
              
              gtk_text_buffer_insert_with_tags (gtk_text_view_get_buffer(textbox), &iter, out, -1,
                                                t_a, color, NULL);
            }
            else if (color != lastcolor)
              gtk_text_buffer_insert_with_tags (gtk_text_view_get_buffer(textbox), &iter, out, -1,
                                                color, NULL);
            else
              gtk_text_buffer_insert (gtk_text_view_get_buffer(textbox), &iter, out, -1);
            
        }
        
    next:
        continue;
    }
    /* scroll to bottom */
    /* gtk_text_thaw (textbox); */
    /* if (bottom) adjust_bottom (textboxadj); */

    g_free(text);
}

/* have to use an idle handler for the adjustment ... or the TextView goes
 * syncronous ... and has bugs.
 * There might be a better way to do a one shot idle handler.
 * This works though */
static GList *adj_list = NULL;

static gboolean cb_adjust_bottom(gpointer data)
{
  GList *scan = adj_list;

  data = data; /* to get rid of the warning */
  
  while (scan)
  {
    GtkTextView *tv = scan->data;
    GtkTextIter iter;

    gtk_text_buffer_get_end_iter(gtk_text_view_get_buffer(tv), &iter);
    /* Maybe want... scroll_to_iter(tv, &iter, 0.0, TRUE, 0.0, 1.0); ???? */
    gtk_text_view_scroll_to_iter(tv, &iter, 0.0, FALSE, 0.0, 0.0);
    
    scan = scan->next;
  }
  
  g_list_free(adj_list);
  adj_list = NULL;
  return FALSE;
}

void adjust_bottom_text_view (GtkTextView *tv)
{
  if (!g_list_find(adj_list, tv))
  {
    if (!adj_list)
      g_idle_add (cb_adjust_bottom, adj_list);
    
    adj_list = g_list_append(adj_list, tv);
  }
}

char *nocolor (char *str)
{
  static GString *ret = NULL;
  size_t len = strlen(str);
  signed char *scan, *p = NULL;
  
  if (!ret)
    ret = g_string_new("");

  g_string_assign(ret, str);

  p = scan = (signed char *)ret->str;
  while (*scan != 0)
  {
    if ((*scan > 0x1F) || (*scan < 0x0)) *p++ = *scan;
    scan++;
  }
  if (scan != p)
    g_string_truncate(ret, len - (scan - p));
  
  return ret->str;
}

/* Check if string is utf-8. If it isn't, convert from locale or iso8859-1. */
gchar* ensure_utf8(const char* str) {
    gchar* text;

    if(g_utf8_validate(str,-1,NULL)) {
        /* The string is valid utf-8, copy it. */
        text=g_strdup(str); 
    } else {
        /* The string isn't valid utf-8, try locale. */
        text=g_locale_to_utf8(str,-1,NULL,NULL,NULL);
        if(!text) { /* The locale didn't work. Use ISO8859-1. */
            text=g_convert(str,-1,"UTF-8","ISO8859-1",NULL,NULL,NULL);
        }
    } 
    /* Any random byte sequence is valid iso8859-1, so this won't happen.*/
    g_assert(text!=NULL && g_utf8_validate(text,-1,NULL));
    return text;
}


