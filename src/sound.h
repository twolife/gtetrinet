#define S_NUM 10

#define S_DROP        0
#define S_SOLIDIFY    1
#define S_LINECLEAR   2
#define S_TETRIS      3
#define S_ROTATE      4
#define S_SPECIALLINE 5
#define S_YOUWIN      6
#define S_YOULOSE     7
#define S_MESSAGE     8
#define S_GAMESTART   9

#define DEFAULTMIDICMD "while true; do playmidi $MIDIFILE; done"

extern int soundenable, midienable;

extern char soundfiles[S_NUM][1024];
extern char midifile[1024];
extern char midicmd[1024];

void sound_cache (void);
void sound_playsound (int id);
void sound_playmidi (char *file);
void sound_stopmidi (void);
