#include <gtk/gtk.h>
GtkWidget *winlist_page_new (void);
void winlist_clear (void);
void winlist_additem (int team, char *name, int score);
void winlist_focus (void);
