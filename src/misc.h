#include <gtk/gtk.h>
extern GtkWidget *leftlabel_new (char *str);
extern void leftlabel_set (GtkWidget *align, char *str);
extern int randomnum (int n);
extern void fdreadline (int fd, char *buf);
extern void textbox_setup (void);
extern void textbox_addtext (GtkText *textbox, unsigned char *text);
extern void adjust_bottom (GtkAdjustment *adj);
extern char *nocolor (char *str);
extern GtkWidget *pixmap_label (GdkPixmap *pm, GdkBitmap *mask, char *str);
extern void move_current_page_to_window (void);
