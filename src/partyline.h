#include <gtk/gtk.h>
extern GtkWidget *partyline_page_new (void);
extern void partyline_connectstatus (int status);
extern void partyline_namelabel (char *nick, char *team);
extern void partyline_status (char *status);
extern void partyline_text (char *text);
extern void partyline_fmt (const char *text, ...) G_GNUC_PRINTF (1, 2);
extern void partyline_playerlist (int *numbers, char **names, char **teams, int n, char **specs, int sn);
extern void partyline_entryfocus (void);
extern void partyline_add_channel (gchar *line);
extern gboolean partyline_update_channel_list (void);
extern void partyline_more_channel_lines (void);
extern void partyline_switch_entryfocus (void);
extern void partyline_clear_list_channel (void);
extern void partyline_joining_channel (const gchar *channel);
extern void stop_list (void);
