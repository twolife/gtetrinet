#include <gtk/gtk.h>

extern GtkTextTagTable *tag_table;

extern GtkWidget *leftlabel_new (char *str);
extern void leftlabel_set (GtkWidget *align, char *str);
extern int randomnum (int n);
extern void fdreadline (int fd, char *buf);
extern void textbox_setup (void);
extern void textbox_addtext (GtkTextView *textbox, const unsigned char *text);
extern void adjust_bottom_text_view (GtkTextView *);
extern char *nocolor (char *str);

/* Better versions of the std. string functions */
#define GTET_STRCPY(x, y, sz) G_STMT_START { \
  size_t gtet_strcpy_x_sz = (sz); \
  size_t gtet_strcpy_y_len = strlen(y); \
  \
  g_assert(gtet_strcpy_x_sz); \
  \
  if (gtet_strcpy_y_len >= gtet_strcpy_x_sz) \
    gtet_strcpy_y_len = gtet_strcpy_x_sz - 1; \
  \
  if (gtet_strcpy_y_len) \
    memcpy((x), (y), gtet_strcpy_y_len); \
  (x)[gtet_strcpy_y_len] = 0; \
 } G_STMT_END

#define GTET_STRCAT(x, y, sz) G_STMT_START { \
  size_t gtet_strcat_x_sz = (sz); \
  size_t gtet_strcat_x_len = strlen(x); \
  size_t gtet_strcat_y_len = strlen(y); \
  \
  g_assert(gtet_strcat_x_sz); \
  \
  if (gtet_strcat_x_len >= (gtet_strcat_x_sz - 1)) \
    gtet_strcat_y_len = 0; \
  \
  gtet_strcat_x_sz -= gtet_strcat_x_len;  \
  if (gtet_strcat_y_len >= gtet_strcat_x_sz) \
    gtet_strcat_y_len = gtet_strcat_x_sz - 1; \
  \
  if (gtet_strcat_y_len) \
    memcpy((x) + gtet_strcat_x_len, (y), gtet_strcat_y_len); \
  (x)[gtet_strcat_x_len + gtet_strcat_y_len] = 0; \
 } G_STMT_END

/* these assume you are passing an "object", Ie. sizeof() returns the true
 * size */
#define GTET_O_STRCPY(x, y) G_STMT_START { \
  g_assert(sizeof(x) > 4); GTET_STRCPY(x, y, sizeof(x)); \
 } G_STMT_END

#define GTET_O_STRCAT(x, y) G_STMT_START { \
  g_assert(sizeof(x) > 4); GTET_STRCAT(x, y, sizeof(x)); \
 } G_STMT_END

/* textbox codes ... */
#define TETRI_TB_RESET 0xFF

#define TETRI_TB_BOLD             2
#define TETRI_TB_ITALIC          22
#define TETRI_TB_UNDERLINE       31

#define TETRI_TB_C_BEG_OFFSET     1 /* in theory 1 and 2 are colors ...
                                     * however 2 == bold */

/* colors... see colors[] in misc.c */
#define TETRI_TB_C_CYAN           3
#define TETRI_TB_C_BLACK          4
#define TETRI_TB_C_BRIGHT_BLUE    5
#define TETRI_TB_C_GREY           6

#define TETRI_TB_C_MAGENTA        8

/* #define TETRI_TB_C_GREY       11 -- dup */
#define TETRI_TB_C_DARK_GREEN    12

#define TETRI_TB_C_BRIGHT_GREEN  14
#define TETRI_TB_C_LIGHT_GREY    15
#define TETRI_TB_C_DARK_RED      16
#define TETRI_TB_C_DARK_BLUE     17
#define TETRI_TB_C_BROWN         18
#define TETRI_TB_C_PURPLE        19
#define TETRI_TB_C_BRIGHT_RED    20
/* #define TETRI_TB_C_LIGHT_GREY 21 -- dup */

#define TETRI_TB_C_DARK_CYAN     23
#define TETRI_TB_C_WHITE         24
#define TETRI_TB_C_YELLOW        25

#define TETRI_TB_C_END_OFFSET    25 /* highest color value */
#define TETRI_TB_END_OFFSET      31 /* highest value - must be less than 32 */
