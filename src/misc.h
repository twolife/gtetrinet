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

/* textbox codes ... */
#define TETRI_TB_BOLD 0x2
#define TETRI_TB_UNDERLINE 0x16

/* colors... see colors[] in misc.c */
#define TETRI_TB_C_CYAN 3
#define TETRI_TB_C_BLACK 4
#define TETRI_TB_C_BRIGHT_BLUE 5
#define TETRI_TB_C_GREY 6

#define TETRI_TB_C_MAGENTA 8

/* #define TETRI_TB_C_GREY 11 -- dup */
#define TETRI_TB_C_DARK_GREEN 12

#define TETRI_TB_C_BRIGHT_GREEN 14
#define TETRI_TB_C_LIGHT_GREY 15
#define TETRI_TB_C_DARK_RED 16
#define TETRI_TB_C_DARK_BLUE 17
#define TETRI_TB_C_BROWN 18
#define TETRI_TB_C_PURPLE 19
#define TETRI_TB_C_BRIGHT_RED 20
/* #define TETRI_TB_C_LIGHT_GREY 21 -- dup */

#define TETRI_TB_C_DARK_CYAN 23
#define TETRI_TB_C_WHITE 24
#define TETRI_TB_C_YELLOW 25
