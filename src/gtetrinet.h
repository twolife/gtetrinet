#define APPID PACKAGE
#define APPNAME "GTetrinet"
#define APPVERSION VERSION

#define ORIGINAL 0
#define TETRIFAST 1

extern int gamemode;

extern GtkWidget *app;

extern void destroymain (void);
extern gint keypress (GtkWidget *widget, GdkEventKey *key);
extern gint keyrelease (GtkWidget *widget, GdkEventKey *key);
extern void move_current_page_to_window (void);
extern void show_fields_page (void);
extern void show_partyline_page (void);
extern void unblock_keyboard_signal (void);
extern gint get_current_notebook_page (void);
