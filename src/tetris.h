typedef char TETRISBLOCK[4][4];
typedef char (*P_TETRISBLOCK)[4];

extern void tetris_drawcurrentblock (void);
extern int tetris_makeblock (int block, int orient);
extern int tetris_randomorient (int block);
extern P_TETRISBLOCK tetris_getblock (int block, int orient);
extern int tetris_blockdown (void);
extern void tetris_blockmove (int dir);
extern void tetris_blockrotate (int dir);
extern void tetris_blockdrop (void);
extern void tetris_solidify (void);
extern void tetris_addlines (int count, int type);
extern int tetris_removelines (char *specials);

extern void copyfield (FIELD dest, FIELD src);
