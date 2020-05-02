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

extern int soundenable;

extern char soundfiles[S_NUM][1024];

void sound_cache (void);
void sound_playsound (int id);

