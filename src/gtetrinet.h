#define APPID PACKAGE
#define APPNAME "GTetrinet"
#define APPVERSION VERSION

extern void destroymain (GtkWidget *widget, gpointer data);
extern gint keypress (GtkWidget *widget, GdkEventKey *key);
extern gint keyrelease (GtkWidget *widget, GdkEventKey *key);

extern GtkWidget *notebook;
