#include <gtk/gtk.h>
extern GtkWidget *partyline_page_new (void);
extern void partyline_connectstatus (int status);
extern void partyline_namelabel (char *nick, char *team);
extern void partyline_status (char *status);
extern void partyline_text (char *text);
extern void partyline_playerlist (int *numbers, char **names, char **teams, int n, char **specs, int sn);
extern void partyline_entryfocus (void);
void partyline_switch_entryfocus (void);
