/*
 *  GTetrinet
 *  Copyright (C) 1999, 2000  Ka-shu Wong (kswong@zip.com.au)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <gtk/gtk.h>
#include <gnome.h>
#include <stdlib.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#include "gtetrinet.h"
#include "config.h"
#include "client.h"
#include "tetrinet.h"
#include "tetris.h"
#include "fields.h"
#include "partyline.h"
#include "winlist.h"
#include "misc.h"
#include "commands.h"
#include "dialogs.h"
#include "sound.h"

#define NEXTBLOCKDELAY (gamemode==TETRIFAST?0:1000)
#define DOWNDELAY 100
#define PARTYLINEDELAY1 100
#define PARTYLINEDELAY2 200

/* all the game state variables goes here */
int playernum = 0;
int moderatornum = 0;
char team[128], nick[128], specpassword[128];

char playernames[7][128];
char teamnames[7][128];
int playerlevels[7];
int playerplaying[7];
int playercount = 0;

char spectatorlist[128][128];
int spectatorcount = 0;

char specialblocks[256];
int specialblocknum = 0;

FIELD fields[7];

int moderator; /* are we the moderator ? TRUE : FALSE */
int spectating; /* are we spectating ? TRUE : FALSE */
int tetrix; /* are we connected to a tetrix server ? TRUE : FALSE */

int linecount, level;
int slines, llines; /* lines still to be used for incrementing level, etc */
int ingame, playing, paused;
int gmsgstate;
int bigfieldnum;

/* game options from the server */
int initialstackheight, initiallevel, linesperlevel, levelinc,
    speciallines, specialcount, specialcapacity, levelaverage, classicmode;

/* these are actually cumulative frequency counts */
int blockfreq[7];
int specialfreq[9];

static char blocks[] = "012345acnrsbgqo";

struct sb {
    char *id;
    int block;
    char *info;
};

#define S_ADDALL1      0
#define S_ADDALL2      1
#define S_ADDALL4      2
#define S_ADDLINE      3
#define S_CLEARLINE    4
#define S_NUKEFIELD    5
#define S_CLEARBLOCKS  6
#define S_SWITCH       7
#define S_CLEARSPECIAL 8
#define S_GRAVITY      9
#define S_BLOCKQUAKE   10
#define S_BLOCKBOMB    11

struct sb sbinfo[] = {
    {"cs1", -1, "1 Line Added to All"},
    {"cs2", -1, "2 Lines Added to All"},
    {"cs4", -1, "4 Lines Added to All"},
    {"a",   6,  "Add Line"},
    {"c",   7,  "Clear Line"},
    {"n",   8,  "Nuke Field"},
    {"r",   9,  "Clear Random Blocks"},
    {"s",  10,  "Switch Fields"},
    {"b",  11,  "Clear Special Blocks"},
    {"g",  12,  "Block Gravity"},
    {"q",  13,  "Blockquake"},
    {"o",  14,  "Block Bomb"},
    {0, 0, 0}
};

static void tetrinet_updatelevels (void);
static void tetrinet_setspeciallabel (char sb);
static void tetrinet_dospecial (int from, int to, int type);
static void tetrinet_specialkey (int pnum);
static void tetrinet_shiftline (int l, int d, FIELD field);
static void moderatorupdate (int now);
static void partylineupdate (int now);
static void partylineupdate_join (char *name);
static void partylineupdate_team (char *name, char *team);
static void partylineupdate_leave (char *name);
static void playerlistupdate (void);
static void fieldslabelupdate (void);
static void plinemsg (char *name, char *text);
static void plineact (char *name, char *text);
static void plinesmsg (char *name, char *text);
static void plinesact (char *name, char *text);
static char translateblock (char c);
static void clearallfields (void);
static void checkmoderatorstatus (void);
static void speclist_clear (void);
static void speclist_add (char *name);
static void speclist_remove (char *name);

static FIELD sentfield; /* the field that the server thinks we have */

/*
 this function processes incoming messages
 */
void tetrinet_inmessage (enum inmsg_type msgtype, char *data)
{
    char buf[1024];
    if (msgtype != IN_PLAYERJOIN && msgtype != IN_PLAYERLEAVE && msgtype != IN_TEAM &&
        msgtype != IN_PLAYERNUM && msgtype != IN_CONNECT && msgtype != IN_F && msgtype != IN_WINLIST) {
        partylineupdate (1);
        moderatorupdate (1);
    }
#ifdef DEBUG
    printf ("%d %s\n", msgtype, data);
#endif
    /* process the message */
    switch (msgtype) {
    case IN_CONNECT:
        /* do nothing - setup is done with the playernum message */
        break;
    case IN_DISCONNECT:
        if (!connected) {
            data = "Server disconnected";
            goto connecterror;
        }
        if (ingame) tetrinet_endgame ();
        connected = ingame = playing = paused = moderator = tetrix = FALSE;
        commands_checkstate ();
        partyline_playerlist (NULL, NULL, 0, NULL, 0);
        partyline_namelabel (NULL, NULL);
        playernum = moderatornum = playercount = spectatorcount = 0;
        {
            /* clear player and team lists */
            int i;
            for (i = 0; i <= 6; i ++)
                playernames[i][0] = teamnames[i][0] = 0;
            fieldslabelupdate ();
        }
        winlist_clear ();
        partyline_text ("\014\02*** Disconnected from server");
        break;
    case IN_CONNECTERROR:
    connecterror:
        {
            GtkWidget *dialog;
            connectingdialog_destroy ();
            strcpy (buf, _("Error connecting: "));
            strcat (buf, data);
            dialog = gnome_message_box_new (buf, GNOME_MESSAGE_BOX_ERROR,
                                            GNOME_STOCK_BUTTON_OK, NULL);
            gtk_widget_show (dialog);
        }
        break;
    case IN_PLAYERNUM:
        bigfieldnum = playernum = atoi (data);
        if (!connected)
        {
            /* we have successfully connected */
            if (spectating) {
                sprintf (buf, "%d %s", playernum, specpassword);
                client_outmessage (OUT_TEAM, buf);
                client_outmessage (OUT_VERSION, APPNAME"-"APPVERSION);
                partyline_namelabel (nick, NULL);
            }
            /* tell everybody of successful connection */
            client_outmessage (OUT_CONNECTED, NULL);
            /* set up stuff */
            connected = TRUE;
            ingame = playing = paused = FALSE;
            playercount = spectating ? 0 : 1;
            partyline_text ("\014\02*** Connected to server");
            commands_checkstate ();
            connectingdialog_destroy ();
            connectdialog_connected ();
            if (spectating) tetrix = TRUE;
            else tetrix = FALSE;
        }
        if (!spectating) {
            /* set own player/team info */
            strcpy (playernames[playernum], nick);
            strcpy (teamnames[playernum], team);
            /* send team info */
            sprintf (buf, "%d %s", playernum, team);
            client_outmessage (OUT_TEAM, buf);
            /* update display */
            playerlistupdate ();
            partyline_namelabel (nick, team);
            fieldslabelupdate ();
            if (connected) {
                checkmoderatorstatus ();
                partylineupdate_join (nick);
                partylineupdate_team (nick, team);
            }
        }
        break;
    case IN_PLAYERJOIN:
        {
            int pnum;
            char *token;
            /* new player has joined */
            token = strtok (data, " ");
            if (token == NULL) break;
            pnum = atoi (token);
            token = strtok (NULL, "");
            if (token == NULL) break;
            if (playernames[pnum][0] == 0) playercount ++;
            strcpy (playernames[pnum], token);
            teamnames[pnum][0] = 0;
            playerlistupdate ();
            /* update fields display */
            fieldslabelupdate ();
            /* display */
            partylineupdate_join (playernames[pnum]);
            /* check moderator status */
            checkmoderatorstatus ();
            /* send out our field */
            /* if (!spectating) tetrinet_resendfield (); */
        }
        break;
    case IN_PLAYERLEAVE:
        {
            int pnum;
            char *token;
            /* player has left */
            token = strtok (data, " ");
            if (token == NULL) break;
            pnum = atoi (token);
            playercount --;
            if (playernames[pnum][0]) {
                /* display */
                partylineupdate_leave (playernames[pnum]);
            }
            /* update playerlist */
            playernames[pnum][0] = 0;
            teamnames[pnum][0] = 0;
            playerlistupdate ();
            /* update fields display */
            fieldslabelupdate ();
            memset (fields[pnum], 0, FIELDHEIGHT*FIELDWIDTH);
            fields_drawfield (playerfield(pnum), fields[pnum]);
            /* check moderator status */
            checkmoderatorstatus ();
        }
        break;
    case IN_KICK:
        {
            int pnum;
            char *token;
            token = strtok (data, " ");
            if (token == NULL) break;
            pnum = atoi (token);
            if ((pnum == playernum) && !spectating)
                sprintf (buf, "\014\02*** You have been kicked from the game");
            else
                sprintf (buf, "\014*** \02%s\377\014 has been kicked from the game", playernames[pnum]);
            partyline_text (buf);
            /*
             mark it so that leave message is not displayed when playerleave
             message is received (see above)
             */
            playernames[pnum][0] = 0;
        }
        break;
    case IN_TEAM:
        {
            int pnum;
            char *token;
            /* parse string */
            token = strtok (data, " ");
            if (token == NULL) break;
            pnum = atoi (token);
            token = strtok (NULL, "");
            if (token == NULL) token = "";
            strcpy (teamnames[pnum], token);
            playerlistupdate ();
            /* update fields display */
            fieldslabelupdate ();
            /* display */
            partylineupdate_team (playernames[pnum], teamnames[pnum]);
        }
        break;
    case IN_PLINE:
        {
            int pnum;
            char *token;
            token = strtok (data, " ");
            if (token == NULL) break;
            pnum = atoi (token);
            token = strtok (NULL, "");
            if (token == NULL) token = "";
            if (pnum == 0) {
                if (strncmp (token, "\04\04\04\04\04\04\04\04", 8) == 0) {
                    /* tetrix identification string */
                    tetrix = TRUE;
                }
                else if (tetrix) {
                    sprintf (buf, "*** %s", token);
                    partyline_text (buf);
                    break;
                }
                else plinemsg ("Server", token);
            }
            else plinemsg (playernames[pnum], token);
        }
        break;
    case IN_PLINEACT:
        {
            int pnum;
            char *token;
            token = strtok (data, " ");
            if (token == NULL) break;
            pnum = atoi (token);
            token = strtok (NULL, "");
            if (token == NULL) token = "";
            /* display it */
            plineact (playernames[pnum], token);
        }
        break;
    case IN_PLAYERLOST:
        {
            int pnum;
            pnum = atoi (data);
            /* player is out */
            playerplaying[pnum] = 0;
        }
        break;
    case IN_PLAYERWON:
        {
            int pnum;
            pnum = atoi (data);
            if (teamnames[pnum][0])
                sprintf (buf, "\020*** Team \02%s\377\020 has won the game",
                         teamnames[pnum]);
            else
                sprintf (buf, "\020*** \02%s\377\020 has won the game",
                         playernames[pnum]);
            partyline_text (buf);
        }
        break;
    case IN_NEWGAME:
        {
            int i, j;
            char bfreq[128], sfreq[128];
            sscanf (data, "%d %d %d %d %d %d %d %s %s %d %d",
                    &initialstackheight, &initiallevel,
                    &linesperlevel, &levelinc, &speciallines,
                    &specialcount, &specialcapacity,
                    bfreq, sfreq, &levelaverage, &classicmode);
            /*
             decoding the 11233345666677777 block frequecy thingies:
             */
            for (i = 0; i < 7; i ++) blockfreq[i] = 0;
            for (i = 0; i < 9; i ++) specialfreq[i] = 0;
            /* count frequencies */
            for (i = 0; bfreq[i]; i ++)
                blockfreq[bfreq[i]-'1'] ++;
            for (i = 0; sfreq[i]; i ++)
                specialfreq[sfreq[i]-'1'] ++;
            /* make it cumulative */
            for (i = 0, j = 0; i < 7; i ++) {
                j += blockfreq[i];
                blockfreq[i] = j;
            }
            for (i = 0, j = 0; i < 9; i ++) {
                j += specialfreq[i];
                specialfreq[i] = j;
            }
            /* sanity checks */
            if (blockfreq[6] < 100) blockfreq[6] = 100;
            if (specialfreq[8] < 100) specialfreq[8] = 100;
            tetrinet_startgame ();
            commands_checkstate ();
            partyline_text ("\024*** The game has \02started");
        }
        break;
    case IN_INGAME:
        {
            FIELD field;
            int x, y, i;
            ingame = TRUE;
            playing = paused = FALSE;
            if (!spectating) {
                for (y = 0; y < FIELDHEIGHT; y ++)
                    for (x = 0; x < FIELDWIDTH; x ++)
                        field[y][x] = randomnum(5) + 1;
                tetrinet_updatefield (field);
                tetrinet_sendfield (1);
                fields_drawfield (playerfield(playernum), fields[playernum]);
            }
            for (i = 0; i <= 6; i ++) playerlevels[i] = -1;
            tetrinet_setspeciallabel (-1);
            fields_gmsginput (FALSE);
            fields_gmsginputclear ();
            commands_checkstate ();
            partyline_text ("\024*** The game is \02in progress");
        }
        break;
    case IN_PAUSE:
	{
	    int newstate = atoi(data);
            /* bail out if no state change */
            if (! (newstate ^ paused)) break;
	    if (newstate) {
		tetrinet_pausegame ();
		partyline_text ("\024*** The game has \02paused");
                fields_attdefmsg ("The game has \02\014Paused");
	    }
	    else {
		tetrinet_resumegame ();
		partyline_text ("\024*** The game has \02resumed");
                fields_attdefmsg ("The game has \02\014Resumed");
	    }
	    commands_checkstate ();
	    break;
	}
    case IN_ENDGAME:
        tetrinet_endgame ();
        commands_checkstate ();
        partyline_text ("\024*** The game has \02ended");
        break;
    case IN_F:
        {
            int pnum;
            char *p, *s;
            s = strtok (data, " ");
            if (s == NULL) break;
            pnum = atoi (s);
            s = strtok (NULL, "");
            if (s == NULL) break;
            if (*s >= '0') {
                /* setting entire field */
                p = (char *)fields[pnum];
                for (; *s; s ++, p ++)
                    *p = translateblock (*s);
            }
            else {
                /* setting specific locations */
                int block = 0, x, y;
                for(; *s; s++) {
                    if (*s < '0' && *s >= '!') block = *s - '!';
                    else {
                        x = *s - '3';
                        y = *(++s) - '3';
                        if (x >= 0 && x < FIELDWIDTH && y >= 0 && y < FIELDHEIGHT)
                            fields[pnum][y][x] = block;
                    }
                }
            }
            if (ingame) fields_drawfield (playerfield(pnum), fields[pnum]);
        }
        break;
    case IN_SB:
        /* special block */
        {
            int sbnum, to, from;
            char *token, *sbid;
            token = strtok (data, " ");
            if (token == NULL) break;
            to = atoi (token);
            sbid = strtok (NULL, " ");
            if (sbid == NULL) break;
            token = strtok (NULL, "");
            if (token == NULL) break;
            from = atoi(token);
            for (sbnum = 0; sbinfo[sbnum].id; sbnum ++)
                if (strcmp (sbid, sbinfo[sbnum].id) == 0) break;
            if (!sbinfo[sbnum].id) break;
            if (ingame) tetrinet_dospecial (from, to, sbnum);
        }
        break;
    case IN_LVL:
        {
            char *token;
            int pnum;
            token = strtok (data, " ");
            if (token == NULL) break;
            pnum = atoi (token);
            token = strtok (NULL, "");
            if (token == NULL) break;
            playerlevels[pnum] = atoi (token);
            if (ingame) tetrinet_updatelevels ();
        }
        break;
    case IN_GMSG:
        fields_gmsgadd (data);
        sound_playsound (S_MESSAGE);
        break;
    case IN_WINLIST:
        {
            char *token, *token2;
            int team, score;
            winlist_clear ();
            token = strtok (data, " ");
            if (token == NULL) break;
            do {
                switch (*token) {
                case 'p': team = FALSE; break;
                case 't': team = TRUE; break;
                default: team = FALSE; break;
                }
                token ++;
                token2 = token;
                while (*token2 != ';' && *token2 != 0) token2 ++;
                *token2 = 0;
                token2 ++;
                score = atoi (token2);
                winlist_additem (team, token, score);
            } while ((token = strtok (NULL, " ")) != NULL);
        }
        break;
    case IN_SPECLIST:
        {
            char *token, buf[1024];
            speclist_clear ();
            token = strtok (data, " ");
            if (token == NULL) break;
            sprintf (buf, "\021*** You have joined \02%s", token);
            partyline_text (buf);
            while ((token = strtok (NULL, " ")) != NULL) speclist_add (token);
            playerlistupdate ();
        }
        break;
    case IN_SPECJOIN:
        {
            char *name, *info, buf[1024];
            name = strtok (data, " ");
            if (name == NULL) break;
            speclist_add (name);
            info = strtok (NULL, "");
            if (info == NULL) info = "";
            sprintf (buf, "\021*** \02%s\377\021 has joined the spectators \06\02(\02%s\337\06\02)", name, info);
            partyline_text (buf);
            playerlistupdate ();
        }
        break;
    case IN_SPECLEAVE:
        {
            char *name, *info, buf[1024];
            name = strtok (data, " ");
            if (name == NULL) break;
            speclist_remove (name);
            info = strtok (NULL, "");
            if (info == NULL) info = "";
            sprintf (buf, "\021*** \02%s\377\021 has left the spectators \06\02(\02%s\337\06\02)", name, info);
            partyline_text (buf);
            playerlistupdate ();
        }
        break;
    case IN_SMSG:
        {
            char *name, *text;
            name = strtok (data, " ");
            if (name == NULL) break;
            text = strtok (NULL, "");
            if (text == NULL) text = "";
            plinesmsg (name, text);
        }
        break;
    case IN_SACT:
        {
            char *name, *text;
            name = strtok (data, " ");
            if (name == NULL) break;
            text = strtok (NULL, "");
            if (text == NULL) text = "";
            plinesact (name, text);
        }
        break;
    default:
    }
}

void tetrinet_playerline (char *text)
{
    char buf[1024], *p;

    if (text[0] == '/') {
        p = text+1;
        if (strncasecmp (p, "me ", 3) == 0) {
            p += 3;
            while (*p && isspace(*p)) p++;
            sprintf (buf, "%d %s", playernum, p);
            client_outmessage (OUT_PLINEACT, buf);
            if (spectating)
                plinesact (nick, p);
            else
                plineact (nick, p);
            return;
        }
        if (tetrix) {
            sprintf (buf, "%d %s", playernum, text);
            client_outmessage (OUT_PLINE, buf);
            return;
        }
    }
    sprintf (buf, "%d %s", playernum, text);
    client_outmessage (OUT_PLINE, buf);
    if (spectating)
        plinesmsg (nick, text);
    else
        plinemsg (nick, text);
}

void tetrinet_changeteam (char *newteam)
{
    char buf[128];

    strcpy (team, newteam);

    if (connected) {
        sprintf (buf, "%d %s", playernum, team);
        client_outmessage (OUT_TEAM, buf);
        tetrinet_inmessage (IN_TEAM, buf);
        partyline_namelabel (nick, team);
    }
}

void tetrinet_sendfield (int reset)
{
    int x, y, i, d = FALSE;
    char buf[1024], buf2[1024], *p;

    if (reset) goto sendwholefield;

    sprintf (buf, "%d ", playernum);
    /* find differences between the fields */
    for (i = 0; i < 15; i ++) {
        p = buf2 + 1;
        buf2[0] = '!' + i;
        for (y = 0; y < FIELDHEIGHT; y ++)
            for (x = 0; x < FIELDWIDTH; x ++)
                if (sentfield[y][x] != fields[playernum][y][x]
                    && fields[playernum][y][x] == i)
                {
                    *p++ = x + '3';
                    *p++ = y + '3';
                }
        if (p > buf2+1) {
            *p = 0;
            strcat (buf, buf2);
            d = TRUE;
        }
    }
    if (!d) return; /* no differences */
    if (strlen (buf) >= FIELDHEIGHT*FIELDWIDTH+2) {
        /* sending entire field is more efficient */
    sendwholefield:
        p = buf2;
        for (y = 0; y < FIELDHEIGHT; y ++)
            for (x = 0; x < FIELDWIDTH; x ++)
                *p++ = blocks[(int)fields[playernum][y][x]];
        *p = 0;
        sprintf (buf, "%d %s", playernum, buf2);
    }
    /* send it */
    client_outmessage (OUT_F, buf);
    /* update the one in our memory */
    copyfield (sentfield, fields[playernum]);
}

void tetrinet_updatefield (FIELD field)
{
    copyfield (fields[playernum], field);
}

void tetrinet_resendfield (void)
{
    char buf[1024], buf2[1024], *p;
    int x, y;
    p = buf2;
    for (y = 0; y < FIELDHEIGHT; y ++)
        for (x = 0; x < FIELDWIDTH; x ++)
            *p++ = blocks[(int)sentfield[y][x]];
    *p = 0;
    sprintf (buf, "%d %s", playernum, buf2);
    client_outmessage (OUT_F, buf);
}

void tetrinet_redrawfields (void)
{
    int i;

    fieldslabelupdate ();
    if (ingame) for (i = 1; i <= 6; i ++)
        fields_drawfield (playerfield(i), fields[i]);
}

static void tetrinet_setspeciallabel (char sb)
{
    int sbnum;
    if (sb == -1) {
        fields_setspeciallabel (_("No special blocks"));
    }
    else {
        for (sbnum = 0; sbinfo[sbnum].id; sbnum ++)
            if (sbinfo[sbnum].block == sb) break;
        fields_setspeciallabel (sbinfo[sbnum].info);
    }

}

/* adds a special block to player's collection */
static void tetrinet_addspecial (char sb)
{
    int l, i;
    if (specialblocknum >= specialcapacity) return; /* too many ! */
    /* add to a random location */
    l = randomnum(specialblocknum);
    for (i = specialblocknum; i > l; i --)
        specialblocks[i] = specialblocks[i-1];
    specialblocks[l] = sb;
    specialblocknum ++;
    tetrinet_setspeciallabel (specialblocks[0]);
}

/* adds a random special block to the field */
static void tetrinet_addsbtofield (int count)
{
    int s, n, c, x, y, i, j;
    char sb;
    FIELD field;
    copyfield (field, fields[playernum]);
    for (i = 0; i < count; i ++) {
        /* get a random special block */
        s = 0;
        n = randomnum(100);
        while (n >= specialfreq[s]) s ++;
        sb = 6 + s;
        /* sb is the special block that we want */
        /* count the number of non-special blocks on the field */
        c = 0;
        for (y = 0; y < FIELDHEIGHT; y ++)
            for (x = 0; x < FIELDWIDTH; x ++)
                if (field[y][x] > 0 && field[y][x] < 6) c ++;
        if (c == 0) { /* drop block */
            /* i *think* this is how it works in the original -
             blocks are not dropped on existing specials,
             and it tries again to find another spot...
             this is because... when a large number of
             blocks are dropped, usually all columns get 1 block
             but sometimes a column or two doesnt get a block */
            for (j = 0; j < 20; j ++) {
                n = randomnum (FIELDWIDTH);
                for (y = 0; y < FIELDHEIGHT; y ++)
                    if (field[y][n]) break;
                if (y == FIELDHEIGHT || field[y][n] < 6) break;
            }
            if (j == 20) goto end;
            y --;
            field[y][n] = sb;
        }
        else { /* choose a random location */
            n = randomnum (c);
            for (y = 0; y < FIELDHEIGHT; y ++)
                for (x = 0; x < FIELDWIDTH; x ++)
                    if (field[y][x] > 0 && field[y][x] < 6) {
                        if (n == 0) {
                            field[y][x] = sb;
                            goto next;
                        }
                        n --;
                    }
        next:
        }
    }
end:
    tetrinet_updatefield (field);
}

/* specials */
void tetrinet_dospecial (int from, int to, int type)
{
    FIELD field;
    int x, y, i;
    char buf[256], buf2[256];

    sprintf (buf, "\02\023%s\023\02", sbinfo[type].info);
    if (to) {
        sprintf (buf2, " on \02\021%s\021\02", playernames[to]);
        strcat (buf, buf2);
    }
    if (from) {
        sprintf (buf2, " by \02\021%s\021\02", playernames[from]);
        strcat (buf, buf2);
    }
    fields_attdefmsg (buf);

    if (!playing) return; /* we're not playing !!! */

    if (from == playernum && type == S_SWITCH) {
        /* we have to switch too... */
        from = to;
        to = playernum;
    }

    if (!(to == 0 && from != playernum) && to != playernum)
        return; /* not for this player */

    if (to == 0)
        /* same team */
        if (team[0] && strcmp (teamnames[from], team) == 0) return;

    copyfield (field, fields[playernum]);

    switch (type)
    {
        /* these add alls need to determine team... */
    case S_ADDALL1:
        tetris_addlines (1, 2);
        break;
    case S_ADDALL2:
        tetris_addlines (2, 2);
        break;
    case S_ADDALL4:
        tetris_addlines (4, 2);
        break;
    case S_ADDLINE:
        tetris_addlines (1, 1);
        break;
    case S_CLEARLINE:
        for (y = FIELDHEIGHT-1; y > 0; y --)
            for (x = 0; x < FIELDWIDTH; x ++)
                field[y][x] = field[y-1][x];
        for (x = 0; x < FIELDWIDTH; x ++) field[0][x] = 0;
        tetrinet_updatefield (field);
        break;
    case S_NUKEFIELD:
        memset (field, 0, FIELDWIDTH*FIELDHEIGHT);
        tetrinet_updatefield (field);
        break;
    case S_CLEARBLOCKS:
        for (i = 0; i < 10; i ++)
            field[randomnum(FIELDHEIGHT)][randomnum(FIELDWIDTH)] = 0;
        tetrinet_updatefield (field);
        break;
    case S_SWITCH:
        copyfield (field, fields[from]);
        for (y = 0; y < 6; y ++)
            for (x = 0; x < FIELDWIDTH; x ++)
                if (field[y][x]) goto bleep;
    bleep:
        i = 6 - y;
        if (i) {
            /* need to move field down i lines */
            for (y = FIELDHEIGHT-1; y >= i; y --)
                for (x = 0; x < FIELDWIDTH; x ++)
                    field[y][x] = field[y-i][x];
            for (y = 0; y < i; y ++)
                for (x = 0; x < FIELDWIDTH; x ++) field[y][x] = 0;
        }
        tetrinet_updatefield (field);
        break;
    case S_CLEARSPECIAL:
        for (y = 0; y < FIELDHEIGHT; y ++)
            for (x = 0; x < FIELDWIDTH; x ++)
                if (field[y][x] > 5) field[y][x] = randomnum (5) + 1;
        tetrinet_updatefield (field);
        break;
    case S_GRAVITY:
        for (y = 0; y < FIELDHEIGHT; y ++)
            for (x = 0; x < FIELDWIDTH; x ++)
                if (field[y][x] == 0) {
                    /* move the above blocks down */
                    for (i = y; i > 0; i --)
                        field[i][x] = field[i-1][x];
                    field[0][x] = 0;
                }
        tetrinet_updatefield (field);
        break;
    case S_BLOCKQUAKE:
        for (y = 0; y < FIELDHEIGHT; y ++) {
            /* [ the original approximation of blockquake frequencies were
                 not quite correct ] */
            /* ### This is a much better approximation and probably how the */
            /* ### original tetrinet does it */
            int s = 0;
            i = randomnum (22);
            if (i < 1) s ++;
            if (i < 4) s ++;
            if (i < 11) s ++;
            if (randomnum(2)) s = -s;
            tetrinet_shiftline (y, s, field);
            /* ### Corrected by Pihvi */
        }
        tetrinet_updatefield (field);
        break;
    case S_BLOCKBOMB:
        {
            int ax[] = {-1, 0, 1, 1, 1, 0, -1, -1};
            int ay[] = {-1, -1, -1, 0, 1, 1, 1, 0};
            int c = 0;
            char block;
            /* find all bomb blocks */
            for (y = 0; y < FIELDHEIGHT; y ++)
                for (x = 0; x < FIELDWIDTH; x ++)
                    if (field[y][x] == 14) {
                        /* remove the bomb */
                        field[y][x] = 0;
                        /* grab the squares around it */
                        for (i = 0; i < 8; i ++) {
                            if (y+ay[i] >= FIELDHEIGHT || y+ay[i] < 0 ||
                                x+ax[i] >= FIELDWIDTH || x+ax[i] < 0) continue;
                            block = field[y+ay[i]][x+ax[i]];
                            if (block == 14) block = 0;
                            buf[c] = block;
                            c ++;
                        }
                    }
            /* scatter blocks */
            for (i = 0; i < c; i ++)
                field[randomnum(FIELDHEIGHT-6)+6][randomnum(FIELDWIDTH)] = buf[i];
        }
        tetrinet_updatefield (field);
        break;
    }
    tetris_removelines (NULL);
    tetrinet_sendfield (0);
    tetris_drawcurrentblock ();
}

static void tetrinet_shiftline (int l, int d, FIELD field)
{
    int i;
    if (d > 0) { /* to the right */
        for (i = FIELDWIDTH-1; i >= d; i --)
            field[l][i] = field[l][i-d];
        for (; i >= 0; i --) field[l][i] = 0;
    }
    if (d < 0) { /* to the left */
        for (i = 0; i < FIELDWIDTH+d; i ++)
            field[l][i] = field[l][i-d];
        for (; i < FIELDWIDTH; i ++) field[l][i] = 0;
    }
    /* if d == 0 do nothing */
}

/* returns a random block */
static int tetrinet_getrandomblock (void)
{
    int i = 0, n = randomnum (100);
    while (n >= blockfreq[i]) i ++;
    return i;
}

static int tetrinet_timeoutduration (void)
{
    return level<=100 ? 1005-level*10 : 5;
}


/*****************/
/* tetrisy stuff */
/*****************/
static void tetrinet_settimeout (int duration);
static gint tetrinet_timeout (gpointer data);
static void tetrinet_solidify (void);
static void tetrinet_nextblock (void);
static gint tetrinet_nextblocktimeout (gpointer data);
static int tetrinet_removelines (void);
static gint tetrinet_removelinestimeout (gpointer data);

TETRISBLOCK blankblock =
{ {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };

gint movedowntimeout = 0, nextblocktimeout = 0;
int downcount = 0, downpressed = 0;
int nextblock, nextorient;

void tetrinet_startgame (void)
{
    int i;
    linecount = slines = llines = 0;
    level = initiallevel;
    for (i = 0; i <= 6; i ++) playerlevels[i] = initiallevel;
    for (i = 0; i <= 6; i ++)
        if (playernames[i][0]) playerplaying[i] = TRUE;
    tetrinet_updatelevels ();
    fields_setlines (0);
    tetrinet_setspeciallabel (-1);
    fields_gmsginput (FALSE);
    fields_gmsginputclear ();
    paused = FALSE;
    specialblocknum = 0;
    ingame = TRUE;
    clearallfields ();
    if (!spectating) {
        playing = TRUE;
        nextblock = tetrinet_getrandomblock ();
        nextorient = tetris_randomorient (nextblock);
        fields_drawnextblock (tetris_getblock(nextblock, nextorient));
        tetris_addlines (initialstackheight, 1);
        tetrinet_sendfield (1);
        tetrinet_nextblock ();
    }
    sound_playsound (S_GAMESTART);
    sound_playmidi (midifile);
}

void tetrinet_pausegame (void)
{
    paused = TRUE;
}

void tetrinet_resumegame (void)
{
    paused = FALSE;
}

void tetrinet_playerlost (void)
{
    int x, y;
    char buf[10];
    FIELD field;
    playing = FALSE;
    /* fix up the display */
    for (y = 0; y < FIELDHEIGHT; y ++)
        for (x = 0; x < FIELDWIDTH; x ++)
            field[y][x] = randomnum(5) + 1;
    tetrinet_updatefield (field);
    fields_drawfield (playerfield(playernum), fields[playernum]);
    fields_drawnextblock (blankblock);
    /* send field */
    tetrinet_sendfield (1);
    /* post message */
    sprintf (buf, "%d", playernum);
    client_outmessage (OUT_PLAYERLOST, buf);
    /* make a sound */
    sound_playsound (S_YOULOSE);
    /* end timeout thingies */
    if (movedowntimeout)
        gtk_timeout_remove (movedowntimeout);
    if (nextblocktimeout)
        gtk_timeout_remove (nextblocktimeout);
    movedowntimeout = nextblocktimeout = 0;
    tetris_makeblock (-1, 0);
}

void tetrinet_endgame (void)
{
    int i, c = 0;
    for (i = 1; i <= 6; i ++)
        if (playerplaying[i]) c ++;
    if (playing && playercount > 1 && c == 1)
        sound_playsound (S_YOUWIN);
    sound_stopmidi ();
    ingame = playing = FALSE;
    if (movedowntimeout)
        gtk_timeout_remove (movedowntimeout);
    if (nextblocktimeout)
        gtk_timeout_remove (nextblocktimeout);
    movedowntimeout = nextblocktimeout = 0;
    tetris_makeblock (-1, 0);
    fields_drawnextblock (blankblock);
    clearallfields ();
    fields_attdefclear ();
    fields_gmsgclear ();
    specialblocknum = 0;
    fields_drawspecials ();
    fields_setlines (-1);
    fields_setlevel (-1);
    fields_setactivelevel (-1);
    fields_setspeciallabel (NULL);
    gmsgstate = 0;
    fields_gmsginput (FALSE);
    fields_gmsginputclear ();
}

void tetrinet_updatelevels (void)
{
    int c = 0, t = 0, i;
    if (levelaverage) {
        /* average levels */
        for (i = 1; i <= 6; i ++) {
            if (playerplaying[i]) {
                c ++;
                t += playerlevels[i];
            }
        }
        if(c) level = t / c;
        fields_setactivelevel (level);
    }
    fields_setlevel (playerlevels[bigfieldnum]);
}

void tetrinet_settimeout (int duration)
{
    if (movedowntimeout)
        gtk_timeout_remove (movedowntimeout);
    movedowntimeout = gtk_timeout_add (duration, (GtkFunction)tetrinet_timeout,
                                       NULL);
}

void tetrinet_removetimeout (void)
{
    if (movedowntimeout)
        gtk_timeout_remove (movedowntimeout);
    movedowntimeout = 0;
}

gint tetrinet_timeout (gpointer data)
{
    if (paused) return TRUE;
    if (!playing) return FALSE;
    if (downpressed) downpressed ++;
    if (tetris_blockdown ()) {
        if (!playing) /* player died within tetris_blockdown() */
            return FALSE;
        if (downcount) {
            tetrinet_solidify ();
            downcount = 0;
        }
        else downcount ++;
    }
    else downcount = 0;
    tetris_drawcurrentblock ();
    return TRUE;
}

void tetrinet_solidify (void)
{
    int sound;
    tetris_solidify ();
    tetrinet_nextblock ();
    sound = tetrinet_removelines ();
    if (sound>=0) sound_playsound (sound);
    else sound_playsound (S_SOLIDIFY);
    tetrinet_sendfield (0);
}

void tetrinet_nextblock (void)
{
    if (nextblocktimeout) return;
    tetrinet_removetimeout ();
    nextblocktimeout =
        gtk_timeout_add (NEXTBLOCKDELAY, (GtkFunction)tetrinet_nextblocktimeout,
                         NULL);
}

gint tetrinet_nextblocktimeout (gpointer data)
{
    if (paused) return TRUE; /* come back later */
    if (!playing) return FALSE;
    if (tetris_makeblock (nextblock, nextorient)) {
        /* player died */
        return FALSE;
    }
    nextblock = tetrinet_getrandomblock ();
    nextorient = tetris_randomorient (nextblock);
    fields_drawnextblock (tetris_getblock(nextblock, nextorient));
    tetris_drawcurrentblock ();
    nextblocktimeout = 0;
    tetrinet_settimeout (tetrinet_timeoutduration());
    return FALSE;
}

int tetrinet_removelines ()
{
    char buf[256];
    int lines, i, j, sound = -1, slcount;
    lines = tetris_removelines (buf);
    if (lines) {
        linecount += lines;
        slines += lines;
        llines += lines;
        fields_setlines (linecount);
        /* save specials */
        for (i = 0; i < lines; i ++)
            for (j = 0; buf[j]; j ++)
                tetrinet_addspecial (buf[j]);
        fields_drawspecials ();
        /* add specials to field */
        slcount = slines / speciallines;
        slines %= speciallines;
        tetrinet_addsbtofield (specialcount * slcount);
        /* work out what noise to make */
        if (lines == 4) sound = S_TETRIS;
        else if (buf[0]) sound = S_SPECIALLINE;
        else sound = S_LINECLEAR;
        /* increment level */
        if (llines >= linesperlevel) {
            char buf[32];
            while (llines >= linesperlevel) {
                playerlevels[playernum] += levelinc;
                llines -= linesperlevel;
            }
            /* tell everybody else */
            sprintf (buf, "%d %d", playernum, playerlevels[playernum]);
            client_outmessage (OUT_LVL, buf);
            tetrinet_updatelevels ();
        }
        /* lines to everyone if in classic mode */
        if (classicmode) {
            int sbnum;
            switch (lines) {
            case 2: sbnum = S_ADDALL1; break;
            case 3: sbnum = S_ADDALL2; break;
            case 4: sbnum = S_ADDALL4; break;
            default: goto endremovelines;
            }
            sprintf (buf, "%i %s %i", 0, sbinfo[sbnum].id, playernum);
            client_outmessage (OUT_SB, buf);
            tetrinet_dospecial (playernum, 0, sbnum);
        }
    endremovelines:
    }
    /* give it a little delay in drawing */
    gtk_timeout_add (40, (GtkFunction)tetrinet_removelinestimeout,
                     NULL);
    return sound;
}

gint tetrinet_removelinestimeout (gpointer data)
{
    tetris_drawcurrentblock ();
    return FALSE;
}

/* return TRUE if key is processed, FALSE if not */
int tetrinet_key (int keyval, char *str)
{
    if (spectating) {
        /* spectator keys */
        switch (keyval) {
        case GDK_1: bigfieldnum = 1; break;
        case GDK_2: bigfieldnum = 2; break;
        case GDK_3: bigfieldnum = 3; break;
        case GDK_4: bigfieldnum = 4; break;
        case GDK_5: bigfieldnum = 5; break;
        case GDK_6: bigfieldnum = 6; break;
        default:    goto notfieldkey;
        }
        tetrinet_updatelevels ();
        tetrinet_redrawfields ();
        return TRUE;
    }
notfieldkey:
    if (!ingame) return FALSE;
    fields_gmsginputactivate (TRUE);
    /* gmsg keys */
    if (gmsgstate == 1) {
        switch (keyval) {
        case GDK_Return:
            {
                char buf[256], *s;
                s = fields_gmsginputtext ();
                if (strlen(s) > 0) {
                    /* post message */
                    sprintf (buf, "<%s> %s", nick, s);
                    client_outmessage (OUT_GMSG, buf);
                }
                /* hide input area */
                fields_gmsginput (FALSE);
                fields_gmsginputclear ();
                gmsgstate = 0;
            }
            break;
        case GDK_Escape:
            gmsgstate = 2;
            fields_gmsginput (FALSE);
            break;
        case GDK_BackSpace:
            fields_gmsginputback ();
            break;
        default:
            fields_gmsginputadd (str);
            break;
        }
        return TRUE;
    }
    if (keyval == keys[K_GAMEMSG]) {
        fields_gmsginput (TRUE);
        gmsgstate = 1;
        return TRUE;
    }
    if (paused || !playing) return FALSE;
    /* game keys */
    if (keyval == keys[K_ROTRIGHT]) {
        if (!nextblocktimeout)
            sound_playsound (S_ROTATE);
        tetris_blockrotate (1);
    }
    else if (keyval == keys[K_ROTLEFT]) {
        if (!nextblocktimeout)
            sound_playsound (S_ROTATE);
        tetris_blockrotate (-1);
    }
    else if (keyval == keys[K_RIGHT]) {
        tetris_blockmove (1);
    }
    else if (keyval == keys[K_LEFT]) {
        tetris_blockmove (-1);
    }
    else if (keyval == keys[K_DOWN]) {
        if (!downpressed) {
            tetrinet_timeout (NULL);
            downpressed = 1;
            tetrinet_settimeout (DOWNDELAY);
        }
    }
    else if (keyval == keys[K_DROP]) {
        int sound;
        if (!nextblocktimeout) {
            tetris_blockdrop ();
            tetris_solidify ();
            tetrinet_nextblock ();
            sound = tetrinet_removelines();
            if (sound >= 0) sound_playsound (sound);
            else sound_playsound (S_DROP);
            tetrinet_sendfield (0);
        }
    }
    else switch (keyval) {
    case GDK_1: tetrinet_specialkey(1); break;
    case GDK_2: tetrinet_specialkey(2); break;
    case GDK_3: tetrinet_specialkey(3); break;
    case GDK_4: tetrinet_specialkey(4); break;
    case GDK_5: tetrinet_specialkey(5); break;
    case GDK_6: tetrinet_specialkey(6); break;
    case GDK_d: tetrinet_specialkey(-1); break;
    default:
        return FALSE;
    }
    tetris_drawcurrentblock ();
    return TRUE;
}

void tetrinet_upkey (int keyval)
{
    if (!playing) return;
    if (keyval == keys[K_DOWN]) {
         /* if it is a quick press, nudge it down one more step */
        if (downpressed == 1) tetrinet_timeout (NULL);
        downpressed = 0;
        tetrinet_settimeout (tetrinet_timeoutduration());
    }
    tetris_drawcurrentblock ();
}

/* called when player uses a special */
void tetrinet_specialkey (int pnum)
{
    char buf[64];
    int sbnum, i;

    if (specialblocknum == 0) return;

    if (pnum != -1 && !playerplaying[pnum]) return;

    /* find which block it is */
    for (sbnum = 0; sbinfo[sbnum].id; sbnum ++)
        if (sbinfo[sbnum].block == specialblocks[0]) break;

    /* remove it from the specials bar */
    for (i = 1; i < specialblocknum; i ++)
        specialblocks[i-1] = specialblocks[i];
    specialblocknum --;
    fields_drawspecials ();
    if (specialblocknum > 0)
        tetrinet_setspeciallabel (specialblocks[0]);
    else
        tetrinet_setspeciallabel (-1);

    /* just discarding a block, no need to say anything */
    if (pnum == -1) return;

    /* send it out */
    sprintf (buf, "%i %s %i", pnum, sbinfo[sbnum].id, playernum);
    client_outmessage (OUT_SB, buf);

    tetrinet_dospecial (playernum, pnum, sbnum);
}

/******************/
/* misc functions */
/******************/

int mutimeout = 0;

/*
 this function exists to prevent the flood of moderator change messages
 that occur on connecting to a server, by combining them to a single line
 */
int moderatorupdate_timeout (void)
{
    if (moderatornum) {
        char buf[256];
        sprintf (buf, "\024*** \02%s\377\024 is the moderator", playernames[moderatornum]);
        partyline_text (buf);
    }
    mutimeout = 0;
    return FALSE;
}

void moderatorupdate (int now)
{
    if (now) {
        if (mutimeout) {
            gtk_timeout_remove (mutimeout);
            moderatorupdate_timeout ();
        }
    }
    else {
        if (mutimeout)
            gtk_timeout_remove (mutimeout);
        mutimeout = gtk_timeout_add (PARTYLINEDELAY2, (GtkFunction)moderatorupdate_timeout, NULL);
    }
}

/* these functions combines consecutive playerjoin and team messages */
char pjoins[16][128];
char pteams[16][128];
int pcount = 0;
char pleaves[16][128];
int plcount = 0;

int putimeout = 0;

int partylineupdate_timeout (void)
{
    int i, j, c;
    char buf[1024], buf2[1024], team[128];
    int f[16];

    if (plcount) {
        strcpy (buf, "\014*** ");
        for (i = 0; i < plcount; i++) {
            sprintf (buf2, "\02%s\377\014, ", pleaves[i]);
            strcat (buf, buf2);
        }
        buf[strlen(buf)-2] = 0;
        if (plcount == 1) strcat (buf, " has left the game");
        else strcat (buf, " have left the game");
        plcount = 0;
        partyline_text (buf);
    }
    if (pcount) {
        strcpy (buf, "\014*** ");
        for (i = 0; i < pcount; i++) {
            sprintf (buf2, "\02%s\377\014, ", pjoins[i]);
            strcat (buf, buf2);
        }
        buf[strlen(buf)-2] = 0;
        if (pcount == 1) strcat (buf, " has joined the game");
        else strcat (buf, " have joined the game");
        partyline_text (buf);

        for (i = 0; i < pcount; i ++) f[i] = 1;
        for (i = 0; i < pcount; i ++) if (f[i]) {
            strcpy (team, pteams[i]);
            sprintf (buf, "\020*** \02%s\377\020", pjoins[i]);
            c = 1;
            for (j = i+1; j < pcount; j ++) {
                if (strcmp (team, pteams[j]) == 0) {
                    sprintf (buf2, ", \02%s\377\020", pjoins[j]);
                    strcat (buf, buf2);
                    f[j] = 0;
                    c ++;
                }
            }
            if (c == 1) strcat (buf, " is ");
            else strcat (buf, " are ");
            if (team[0]) sprintf (buf2, "on team \02%s", team);
            else sprintf (buf2, "alone");
            strcat (buf, buf2);
            partyline_text (buf);
        }
        pcount = 0;
    }
    
    putimeout = 0;
    return FALSE;
}

void partylineupdate (int now)
{
    if (putimeout) gtk_timeout_remove (putimeout);
    if (now)
        partylineupdate_timeout ();
    else
        putimeout = gtk_timeout_add (PARTYLINEDELAY1, (GtkFunction)partylineupdate_timeout,
                                     NULL);
}

void partylineupdate_join (char *name)
{
    int i;
    if (!connected) return;
    for (i = 0; i < pcount; i ++) if (strcmp(pjoins[i], name) == 0) return;
    strcpy (pjoins[pcount], name);
    pteams[pcount][0] = 0;
    pcount ++;
    partylineupdate (0);
}

void partylineupdate_team (char *name, char *team)
{
    int i;
    if (!connected) return;
    for (i = 0; i < pcount; i ++)
        if (strcmp (pjoins[i], name) == 0) break;
    if (i == pcount) {
        char buf[1024];
        /* player did not just join - display normally */
        if (team[0])
            sprintf (buf, "\020*** \02%s\377\020 is now on team \02%s",
                     name, team);
        else
            sprintf (buf, "\020*** \02%s\377\020 is now alone",
                     name);
        partyline_text (buf);
    }
    strcpy (pteams[i], team);
    partylineupdate (0);
}

void partylineupdate_leave (char *name)
{
    if (!connected) return;
    strcpy (pleaves[plcount], name);
    plcount ++;
    partylineupdate (0);
}

void playerlistupdate (void)
{
    int i, sn, n = 0;
    char *pnames[6], *teams[6], *specs[128];
    for (i = 1; i <= 6; i ++) {
        if (playernames[i][0]) {
            pnames[n] = playernames[i];
            teams[n] = teamnames[i];
            n ++;
        }
    }
    for (sn = 0; sn < spectatorcount; sn++) specs[sn] = spectatorlist[sn];
    partyline_playerlist (pnames, teams, n, specs, sn);
}

void fieldslabelupdate (void)
{
    int i;
    for (i = 1; i <= 6; i ++) {
        if (playernames[i][0] == 0)
            fields_setlabel (playerfield(i), NULL, NULL, 0);
        else
            fields_setlabel (playerfield(i), playernames[i], teamnames[i], i);
    }
}

void checkmoderatorstatus (void)
{
    int i;
    /* find the lowest numbered player, or 0 if no players exist */
    for (i = 1; i <= 6; i ++)
        if (playernames[i][0]) break;
    if (i > 6) i = 0;
    if (!spectating && (i == playernum)) moderator = TRUE;
    else moderator = FALSE;
    commands_checkstate ();
    if (moderatornum != i) {
        moderatornum = i;
        moderatorupdate (0);
    }
}

void plinemsg (char *name, char *text)
{
    char buf[1024];
    sprintf (buf, "\02<%s\377\02>\02 %s", name, text);
    partyline_text (buf);
}

void plinesmsg (char *name, char *text)
{
    char buf[1024];
    sprintf (buf, "\05\02<%s\377\05\02>\02 %s", name, text);
    partyline_text (buf);
}

void plineact (char *name, char *text)
{
    char buf[1024];
    sprintf (buf, "\023* \02%s\377\023 %s", name, text);
    partyline_text (buf);
}

void plinesact (char *name, char *text)
{
    char buf[1024];
    sprintf (buf, "\05* \02%s\377\05 %s", name, text);
    partyline_text (buf);
}

char translateblock (char c)
{
    int i;
    for (i = 0; blocks[i]; i ++)
        if (c == blocks[i]) return i;
    return 0;
}

int playerfield (int p)
{
    if (p < bigfieldnum) return p;
    else if (p == bigfieldnum) return 0;
    else return p - 1;
}

void clearallfields (void)
{
    int i;
    memset (fields, 0, 7*FIELDHEIGHT*FIELDWIDTH);
    for (i = 1; i <= 6; i ++)
        fields_drawfield (playerfield(i), fields[i]);
}

void speclist_clear (void)
{
    spectatorcount = 0;
}

void speclist_add (char *name)
{
    int p, i;
    for (p = 0; p < spectatorcount; p++)
        if (strcasecmp(name, spectatorlist[p]) < 0) break;
    for (i = spectatorcount; i > p; i--)
        strcpy (spectatorlist[i], spectatorlist[i-1]);
    strcpy (spectatorlist[p], name);
    spectatorcount ++;
}

void speclist_remove (char *name)
{
    int i;
    for (i = 0; i < spectatorcount; i ++) {
        if (strcmp(name, spectatorlist[i]) == 0) {
            for (; i < spectatorcount-1; i++)
                strcpy (spectatorlist[i], spectatorlist[i+1]);
            spectatorcount --;
            return;
        }
    }
}
