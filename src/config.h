extern char blocksfile[1024];
extern int bsize;
extern char currenttheme[1024];
extern gint keys[];
extern gint defaultkeys[];

extern void config_loadtheme (char *themedir);
extern int config_getthemeinfo (char *themedir, char *name, char *author, char *desc);
extern void config_loadconfig (void);
extern void config_saveconfig (void);

#define GTETRINET_THEMES GTETRINET_DATA"/themes"
#define DEFAULTTHEME GTETRINET_THEMES"/default/"

#define K_RIGHT 0
#define K_LEFT 1
#define K_ROTRIGHT 2
#define K_ROTLEFT 3
#define K_DOWN 4
#define K_DROP 5
#define K_GAMEMSG 6
/* not a key but the number of configurable keys */
#define K_NUM 7
