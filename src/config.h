#include <gconf/gconf-client.h>

extern char blocksfile[1024];
extern int bsize;
extern GString *currenttheme;
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

void
keys_special1_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry);

void
keys_special2_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry);

void
keys_special3_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry);

void
keys_special4_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry);

void
keys_special5_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry);

void
keys_special6_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry);

void
partyline_enable_timestamps_changed (GConfClient *client,
                                     guint cnxn_id,
                                     GConfEntry *entry);

void
partyline_enable_channel_list_changed (GConfClient *client,
                                       guint cnxn_id,
                                       GConfEntry *entry);


#define GTETRINET_THEMES GTETRINET_DATA"/themes"
#define DEFAULTTHEME GTETRINET_THEMES"/default/"

typedef enum
{
  K_RIGHT, 
  K_LEFT, 
  K_ROTRIGHT, 
  K_ROTLEFT,
  K_DOWN,
  K_DROP,
  K_DISCARD,
  K_GAMEMSG,
  K_SPECIAL1,
  K_SPECIAL2,
  K_SPECIAL3, 
  K_SPECIAL4,
  K_SPECIAL5,
  K_SPECIAL6,
/* not a key but the number of configurable keys */
  K_NUM
} GTetrinetKeys;
