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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>

#include "client.h"
#include "tetrinet.h"
#include "partyline.h"
#include "dialogs.h"
#include "misc.h"
#include "gtetrinet.h"

#define PORT 31457
#define SPECPORT 31458

int fdin[2], fdout[2]; /* two pipes, in and out */

int inputcbid;

int connected;
char server[128];
int clientpid;

static int sock, connecterror;

/* structures and arrays for message translation */

struct inmsgt {
    enum inmsg_type num;
    char *str;
};

struct outmsgt {
    enum outmsg_type num;
    char *str;
};

/* some of these strings change depending on the game mode selected */
/* these changes are put into effect through the function inmsg_change */
struct inmsgt inmsgtable[] = {
    {IN_CONNECT, "connect"},
    {IN_DISCONNECT, "disconnect"},

    {IN_CONNECTERROR, "noconnecting"},
    {IN_PLAYERNUM, "playernum"},
    {IN_PLAYERJOIN, "playerjoin"},
    {IN_PLAYERLEAVE, "playerleave"},
    {IN_KICK, "kick"},
    {IN_TEAM, "team"},
    {IN_PLINE, "pline"},
    {IN_PLINEACT, "plineact"},
    {IN_PLAYERLOST, "playerlost"},
    {IN_PLAYERWON, "playerwon"},
    {IN_NEWGAME, "newgame"},
    {IN_INGAME, "ingame"},
    {IN_PAUSE, "pause"},
    {IN_ENDGAME, "endgame"},
    {IN_F, "f"},
    {IN_SB, "sb"},
    {IN_LVL, "lvl"},
    {IN_GMSG, "gmsg"},
    {IN_WINLIST, "winlist"},

    {IN_SPECJOIN, "specjoin"},
    {IN_SPECLEAVE, "specleave"},
    {IN_SPECLIST, "speclist"},
    {IN_SMSG, "smsg"},
    {IN_SACT, "sact"},
    {0, 0}
};

static struct inmsgt *get_inmsg_entry(enum inmsg_type num)
{
    int i;
    for (i = 0; inmsgtable[i].num && inmsgtable[i].num != num; i ++);
    return &inmsgtable[i];
}

static void inmsg_change()
{
    switch (gamemode) {
    case ORIGINAL:
        get_inmsg_entry(IN_PLAYERNUM)->str = "playernum";
        get_inmsg_entry(IN_NEWGAME)->str = "newgame";
        break;
    case TETRIFAST:
        get_inmsg_entry(IN_PLAYERNUM)->str = ")#)(!@(*3";
        get_inmsg_entry(IN_NEWGAME)->str = "*******";
        break;
    }
}

struct outmsgt outmsgtable[] = {
    {OUT_DISCONNECT, "disconnect"},
    {OUT_CONNECTED, "connected"},

    {OUT_TEAM, "team"},
    {OUT_PLINE, "pline"},
    {OUT_PLINEACT, "plineact"},
    {OUT_PLAYERLOST, "playerlost"},
    {OUT_F, "f"},
    {OUT_SB, "sb"},
    {OUT_LVL, "lvl"},
    {OUT_STARTGAME, "startgame"},
    {OUT_PAUSE, "pause"},
    {OUT_GMSG, "gmsg"},

    {OUT_VERSION, "version"},
    {0, 0}
};

static void client_inputfunc (gpointer data, gint source,
                              GdkInputCondition condition);

/* functions which set up the connection */
static void client_process (void);
static void client_cleanpipe (void);
static int client_mainloop (void);
static int client_connect (void);
static void client_connected (void);
static int client_disconnect (void);

/* some other useful functions */
static int client_sendmsg (char *str);
static int client_readmsg (char *str, int len);
static void server_ip (unsigned char buf[4]);

enum inmsg_type inmsg_translate (char *str);
char *outmsg_translate (enum outmsg_type);

void client_initpipes (void)
{
    pipe (fdin);
    pipe (fdout);

    inputcbid = gdk_input_add (fdin[0], GDK_INPUT_READ,
                               (GdkInputFunction)client_inputfunc, NULL);
}

void client_destroypipes (void)
{
}

void client_init (char *s, char *n)
{
    int i;
    strcpy (server, s);
    strcpy (nick, n);

    /* wipe spaces off the nick */
    for (i = 0; nick[i]; i ++)
        if (isspace (nick[i])) nick[i] = 0;

    if (clientpid) {
        if (connected) client_destroy ();
        else client_connectcancel ();
    }

    connectingdialog_new ();

    /* set the game mode */
    inmsg_change();

    if ((clientpid = fork()) == 0) {
        client_process ();
        _exit (0);
    }
}

void client_connectcancel (void)
{
    /* just kill the process */
    if (clientpid) kill (clientpid, SIGTERM);
    clientpid = 0;
}

void client_destroy (void)
{
    /* tell it to disconnect, then wait for it to die */
    client_outmessage (OUT_DISCONNECT, NULL);
    waitpid (clientpid, NULL, 0);
    clientpid = 0;
}

/* a function that reads stuff from the pipe */
static void client_inputfunc (gpointer data, gint source,
                              GdkInputCondition condition)
{
    char buf[1024];
    char *token;
    enum inmsg_type msgtype;

    fdreadline (fdin[0], buf);

    /* split the message */
    token = strtok (buf, " ");
    if (token == NULL) return;
    msgtype = inmsg_translate (token);
    token = strtok (NULL, "");

    /* process it */
    tetrinet_inmessage (msgtype, token);
}

void client_outmessage (enum outmsg_type msgtype, char *str)
{
    char buf[1024];
    strcpy (buf, outmsg_translate (msgtype));
    if (str) {
        strcat (buf, " ");
        strcat (buf, str);
    }
    write (fdout[1], buf, strlen(buf));
    write (fdout[1], "\n", 1);
}

void client_inmessage (char *str)
{
    write (fdin[1], str, strlen(str));
    write (fdin[1], "\n", 1);
}

/* these functions set up the connection */

void client_process (void)
{
    errno = 0;
    if (client_connect () == -1) {
        char errmsg[1024];

        strcpy (errmsg, "noconnecting ");

        if (errno) strcat (errmsg, strerror (errno));
        else if (h_errno) strcat (errmsg, "Unknown host");

        client_inmessage (errmsg);

        return;
    }
    connecterror = 0;
    client_cleanpipe ();
    client_mainloop ();
    client_disconnect ();
    return;
}

void client_cleanpipe ()
{
    fd_set rfds;
    struct timeval tv;
    char buf[1024];

    while (1) {
        FD_ZERO (&rfds);
        FD_SET (fdout[0], &rfds);
        tv.tv_sec = 0; tv.tv_usec = 0;
        if (select (fdout[0]+1, &rfds, NULL, NULL, &tv)) fdreadline (fdout[0], buf);
        else return;
    }
}

int client_mainloop (void)
{
    fd_set rfds;
    struct timeval tv;
    int m;

    /* read in stuff from the socket and the pipe */
    while (1) {
        FD_ZERO (&rfds);
        FD_SET (sock, &rfds);
        FD_SET (fdout[0], &rfds);
        m = sock > fdout[0] ? sock : fdout[0];
        tv.tv_sec = 0;
        tv.tv_usec = 50000; /* wait a little while */
        if (select (m+1, &rfds, NULL, NULL, &tv))
        {
            char buf[1024]; /* a big buffer */

            /* we have something to read */

            /* is it the socket?? */
            FD_ZERO (&rfds); FD_SET (sock, &rfds);
            tv.tv_sec = 0; tv.tv_usec = 0;
            if (select (sock+1, &rfds, NULL, NULL, &tv)) {
                if (client_readmsg (buf, sizeof(buf)) < 0)
                    return -1;
                client_inmessage (buf); /* send to parent process */
                if (strncmp ("noconnecting", buf, 12) == 0) {
                    connecterror = 1;
                    goto clientend;
                }
            }

            /* or it the pipe?? */
            FD_ZERO (&rfds); FD_SET (fdout[0], &rfds);
            tv.tv_sec = 0; tv.tv_usec = 0;
            if (select (fdout[0]+1, &rfds, NULL, NULL, &tv)) {
                fdreadline (fdout[0], buf);
                if (strcmp (buf, outmsg_translate(OUT_DISCONNECT)) == 0)
                    goto clientend;
                else if (strcmp (buf, outmsg_translate(OUT_CONNECTED)) == 0)
                    client_connected ();
                else
                    client_sendmsg (buf);
            }
        }
    }
clientend:
    return 0;
}

int client_connect (void)
{
    struct hostent *h;
    struct sockaddr_in sa;

    /* set up the connection */

    h = gethostbyname (server);
    if (h == 0) {
        /* set errno = 0 so that we know it's a gethostbyname error */
        errno = 0;
        return -1;
    }
    memset (&sa, 0, sizeof (sa));
    memcpy (&sa.sin_addr, h->h_addr, h->h_length);
    sa.sin_family = h->h_addrtype;
    sa.sin_port = htons (spectating?SPECPORT:PORT);

    sock = socket (sa.sin_family, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    if (connect (sock, (struct sockaddr *)&sa, sizeof(sa)) < 0)
        return -1;

    /* say hello to the server */
    {
        char buf[200], buf2[200];
        unsigned char ip[4];
        unsigned char iphashbuf[6];
        int i, l, len;
        /* construct message */
        if (gamemode == TETRIFAST)
            sprintf (buf, "tetrifaster %s 1.13", nick);
        else
            sprintf (buf, "tetrisstart %s 1.13", nick);
        /* do that encoding thingy */
        server_ip (ip);
        sprintf (iphashbuf, "%d", ip[0]*54 + ip[1]*41 + ip[2]*29 + ip[3]*17);
        l = strlen (iphashbuf);
        buf2[0] = 0;
        for (i = 0; buf[i]; i ++)
            buf2[i+1] = (((buf2[i]&0xFF)+(buf[i]&0xFF))%255) ^ iphashbuf[i%l];
        len = i + 1;
        for (i = 0; i < len; i ++)
            sprintf (buf+i*2, "%02X", buf2[i] & 0xFF);
        /* now send to server */
        client_sendmsg (buf);
    }
    return 0;
}

void client_connected (void)
{
    connected = 1;
    client_inmessage ("connect");
}

int client_disconnect (void)
{
    shutdown (sock, 2);
    close (sock);

    if (!connecterror || connected) client_inmessage ("disconnect");

    return 0;
}


/* some other useful functions */

int client_sendmsg (char *str)
{
    char c = 0xFF;
    
#ifdef DEBUG
    printf ("> %s\n", str);
#endif

    if (write (sock, str, strlen (str))==-1) return -1;
    if (write (sock, &c, 1)==-1) return -1;

    return 0;
}

int client_readmsg (char *str, int len)
{
    int i = 0;
    /* read it in one char at a time */
    for (;i < len-1; i++) {
        if (read (sock, &str[i], 1) != 1)
            /* we have an error */
            return -1;
        if (str[i]==(char)0xFF) break;
    }
    str[i] = 0;

#ifdef DEBUG
    printf ("< %s\n", str);
#endif

    return i;
}

void server_ip (unsigned char buf[4])
{
    struct sockaddr_in sin;
    int len = sizeof(sin);

    getpeername (sock, (struct sockaddr *)&sin, &len);
    memcpy (buf, &sin.sin_addr, 4);
}

enum inmsg_type inmsg_translate (char *str)
{
    int i;
    for (i = 0; inmsgtable[i].str; i++) {
        if (strcmp (inmsgtable[i].str, str) == 0)
            return inmsgtable[i].num;
    }
    return 0;
}

char *outmsg_translate (enum outmsg_type num)
{
    int i;
    for (i = 0; outmsgtable[i].num; i++) {
        if (outmsgtable[i].num==num) return outmsgtable[i].str;
    }
    return NULL;
}
