#include "../config.h"

extern GnomeUIInfo menubar[];
extern GnomeUIInfo toolbar[];

void make_menus (GnomeApp *app);

void connect_command (GtkWidget *widget, gpointer data);
void disconnect_command (GtkWidget *widget, gpointer data);
void team_command (GtkWidget *widget, gpointer data);
#ifdef ENABLE_DETACH
void detach_command (GtkWidget *widget, gpointer data);
#endif
void start_command (GtkWidget *widget, gpointer data);
void end_command (GtkWidget *widget, gpointer data);
void pause_command (GtkWidget *widget, gpointer data);
void preferences_command (GtkWidget *widget, gpointer data);
void about_command (GtkWidget *widget, gpointer data);

void commands_checkstate (void);
