#include <gconf/gconf-client.h>

extern char blocksfile[1024];
extern int bsize;
extern gchar currenttheme[1024];
extern guint keys[];
extern guint defaultkeys[];

extern void config_loadtheme (const gchar *themedir);
extern int config_getthemeinfo (char *themedir, char *name, char *author, char *desc);
extern void config_loadconfig (void);
extern void config_saveconfig (void);

void
sound_midi_player_changed (GConfClient *client,
                           guint cnxn_id,
                           GConfEntry *entry);

void
sound_enable_sound_changed (GConfClient *client,
                            guint cnxn_id,
                            GConfEntry *entry);

void
sound_enable_midi_changed (GConfClient *client,
                           guint cnxn_id,
                           GConfEntry *entry);

void
themes_theme_dir_changed (GConfClient *client,
                          guint cnxn_id,
                          GConfEntry *entry);
                          
void
keys_down_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry);

void
keys_left_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry);

void
keys_right_changed (GConfClient *client,
                    guint cnxn_id,
                    GConfEntry *entry);

void
keys_drop_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry);

void
keys_rotate_left_changed (GConfClient *client,
                          guint cnxn_id,
                          GConfEntry *entry);

void
keys_rotate_right_changed (GConfClient *client,
                           guint cnxn_id,
                           GConfEntry *entry);

void
keys_message_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry);

void
keys_discard_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry);

#define GTETRINET_THEMES GTETRINET_DATA"/themes"
#define DEFAULTTHEME GTETRINET_THEMES"/default/"

#define K_RIGHT 0
#define K_LEFT 1
#define K_ROTRIGHT 2
#define K_ROTLEFT 3
#define K_DOWN 4
#define K_DROP 5
#define K_DISCARD 6
#define K_GAMEMSG 7
/* not a key but the number of configurable keys */
#define K_NUM 8
