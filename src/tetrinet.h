#define FIELDWIDTH 12
#define FIELDHEIGHT 22

typedef char FIELD[FIELDHEIGHT][FIELDWIDTH];

extern char pnumrec;

extern int playernum;
extern char team[128], nick[128], specpassword[128];
extern FIELD fields[7];
extern int ingame, playing, paused;
extern int moderator, spectating;
extern int bigfieldnum;
extern int gmsgstate;

extern char specialblocks[256];
extern int specialblocknum;
extern int list_issued;

extern void tetrinet_inmessage (enum inmsg_type msgtype, char *data);
extern void tetrinet_playerline (const char *text);
extern void tetrinet_changeteam (const char *newteam);
extern void tetrinet_updatefield (FIELD field);
extern void tetrinet_sendfield (int reset);
extern void tetrinet_resendfield (void);
extern void tetrinet_redrawfields (void);
extern int tetrinet_key (int keyval);
extern void tetrinet_upkey (int keyval);

extern void tetrinet_startgame (void);
extern void tetrinet_pausegame (void);
extern void tetrinet_resumegame (void);
extern void tetrinet_playerlost (void);
extern void tetrinet_endgame (void);

extern int playerfield (int p);

extern void fieldslabelupdate (void);
