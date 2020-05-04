#define GSETTINGS_DOMAIN "gtetrinet"
#define GSETTINGS_DOMAIN_KEYS "gtetrinet.keys"
#define GSETTINGS_DOMAIN_THEMES "gtetrinet.themes"

extern char blocksfile[1024];
extern int bsize;
extern GString *currenttheme;
extern guint keys[];
extern guint defaultkeys[];

extern void config_loadtheme (const gchar *themedir);
extern int config_getthemeinfo (char *themedir, char *name, char *author, char *desc);
extern void config_loadconfig (void);
extern void config_loadconfig_keys (void);
extern void config_loadconfig_themes (void);

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
