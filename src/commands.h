extern GnomeUIInfo menubar[];
extern GnomeUIInfo toolbar[];

void make_menus (GnomeApp *app);

void connect_command (void);
void disconnect_command (void);
void team_command (void);
#ifdef ENABLE_DETACH
void detach_command (void);
#endif
void start_command (void);
void end_command (void);
void pause_command (void);
void preferences_command (void);
void about_command (void);

void commands_checkstate (void);
