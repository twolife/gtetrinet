/*
 *  GTetrinet
 *  Copyright (C) 1999, 2000, 2001, 2002, 2003  Ka-shu Wong (kswong@zip.com.au)
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

#ifdef HAVE_CONFIG_H
# include "../config.h"
#endif

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
#include <gnome.h>

#include "client.h"
#include "tetrinet.h"
#include "partyline.h"
#include "dialogs.h"
#include "misc.h"
#include "gtetrinet.h"

#define PORT 31457
#define SPECPORT 31458

int connected;
char server[128];

static int sock;
static GIOChannel *io_channel;
static guint source;
static int resolved;

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

/* functions which set up the connection */
static void client_process (void);
static gpointer client_resolv_hostname (void);
static void client_connected (void);

/* some other useful functions */
static gboolean io_channel_cb (GIOChannel *source, GIOCondition condition);
static int client_sendmsg (char *str);
static int client_readmsg (gchar **str);
static void server_ip (unsigned char buf[4]);

enum inmsg_type inmsg_translate (char *str);
char *outmsg_translate (enum outmsg_type);

void client_init (const char *s, const char *n)
{
    int i;
    GTET_O_STRCPY(server, s);
    GTET_O_STRCPY(nick, n);

    connectingdialog_new ();

    /* wipe spaces off the nick */
    for (i = 0; nick[i]; i ++)
      if (isspace (nick[i]))
        nick[i] = 0;

    /* set the game mode */
    inmsg_change();
    
    client_process ();
}

void client_outmessage (enum outmsg_type msgtype, char *str)
{
    char buf[1024];
    GTET_O_STRCPY(buf, outmsg_translate (msgtype));
    if (str) {
        GTET_O_STRCAT(buf, " ");
        GTET_O_STRCAT(buf, str);
    }
    switch (msgtype)
    {
      case OUT_DISCONNECT : client_disconnect (); break;
      case OUT_CONNECTED : client_connected (); break;
      default : client_sendmsg (buf);
    }
}

void client_inmessage (char *str)
{
    enum inmsg_type msgtype;
    gchar **tokens, *final;

    /* split the message */
    tokens = g_strsplit (str, " ", 256);
    msgtype = inmsg_translate (tokens[0]);

    /* process it */
    final = g_strjoinv (" ", &tokens[1]);
    tetrinet_inmessage (msgtype, final);
    g_strfreev (tokens);
    g_free (final);
}

/* these functions set up the connection */

void client_process (void)
{
  GString *s1 = g_string_sized_new(80);
  GString *s2 = g_string_sized_new(80);
  unsigned char ip[4];
  GString *iphashbuf = g_string_sized_new(11);
  unsigned int i, len;
  int l;
  GThread *thread;
        
  errno = 0;
  resolved = 0;
  
  thread = g_thread_create ((GThreadFunc) client_resolv_hostname, NULL, FALSE, NULL);
  
  /* wait until the hostname is resolved */
  while (resolved == 0)
  {
    if (gtk_events_pending ())
	    gtk_main_iteration ();
  }

  if (resolved == -1) {
    char errmsg[1024];

    GTET_O_STRCPY(errmsg, "noconnecting ");

    if (errno)        GTET_O_STRCAT(errmsg, strerror (errno));
    else if (h_errno) GTET_O_STRCAT(errmsg, _("Couldn't resolve hostname."));

    client_inmessage (errmsg);
    
    return;
 }

  /**
   * Set up the g_io_channel
   * We should set it with no encoding and no buffering, just to simplify things */
  io_channel = g_io_channel_unix_new (sock);
  g_io_channel_set_encoding (io_channel, NULL, NULL);
  g_io_channel_set_buffered (io_channel, FALSE);
  source = g_io_add_watch (io_channel, G_IO_IN, (GIOFunc)io_channel_cb, NULL);

  /* construct message */
  if (gamemode == TETRIFAST)
    g_string_printf (s1, "tetrifaster %s 1.13", nick);
  else
    g_string_printf (s1, "tetrisstart %s 1.13", nick);

  /* do that encoding thingy */
  server_ip (ip);
  g_string_printf (iphashbuf, "%d",
                   ip[0]*54 + ip[1]*41 + ip[2]*29 + ip[3]*17);
  l = iphashbuf->len;

  g_string_append_c(s2, 0);
  for (i = 0; s1->str[i]; i ++)
    g_string_append_c(s2, ((((s2->str[i] & 0xFF) +
                     (s1->str[i] & 0xFF)) % 255) ^
                      iphashbuf->str[i % l]));
  g_assert(s1->len == i);
  g_assert(s2->len == (i + 1));
  len = i + 1;

  g_string_truncate(s1, 0);
  for (i = 0; i < len; i ++)
    g_string_append_printf(s1, "%02X", s2->str[i] & 0xFF);

  /* now send to server */
  client_sendmsg (s1->str);

  g_string_free(s1, TRUE);
  g_string_free(s2, TRUE);
  g_string_free(iphashbuf, TRUE);
}


gpointer client_resolv_hostname (void)
{
#ifdef USE_IPV6
    char hbuf[NI_MAXHOST];
    struct addrinfo hints, *res, *res0;
    struct sockaddr_in6 sa;
    char service[10];
#else
    struct hostent *h;
    struct sockaddr_in sa;
#endif

    /* set up the connection */

#ifdef USE_IPV6
    snprintf(service, 9, "%d", spectating?SPECPORT:PORT);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(server, service, &hints, &res0)) {
        /* set errno = 0 so that we know it's a getaddrinfo error */
        errno = 0;
        resolved = -1;
        g_thread_exit (GINT_TO_POINTER (-1));
    }
    for (res = res0; res; res = res->ai_next) {
        sock = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock < 0) {
            if (res->ai_next)
                continue;
            else {
                freeaddrinfo(res0);
                resolved = -1;
                g_thread_exit (GINT_TO_POINTER (-1));
            }
        }
        getnameinfo(res->ai_addr, res->ai_addrlen, hbuf, sizeof(hbuf), NULL, 0, 0);
        if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
            if (res->ai_next) {
                close(sock);
                continue;
            } else {
                close(sock);
                freeaddrinfo(res0);
                resolved = -1;
                g_thread_exit (GINT_TO_POINTER (-1));
            }
        }
        break;
    }
    freeaddrinfo(res0);
#else
    h = gethostbyname (server);
    if (h == 0) {
        /* set errno = 0 so that we know it's a gethostbyname error */
        errno = 0;
        resolved = -1;
        g_thread_exit (GINT_TO_POINTER (-1));
    }
    memset (&sa, 0, sizeof (sa));
    memcpy (&sa.sin_addr, h->h_addr, h->h_length);
    sa.sin_family = h->h_addrtype;
    sa.sin_port = htons (spectating?SPECPORT:PORT);

    sock = socket (sa.sin_family, SOCK_STREAM, 0);
    if (sock < 0)
        g_thread_exit (GINT_TO_POINTER (-1));

    if (connect (sock, (struct sockaddr *)&sa, sizeof(sa)) < 0)
    {
        resolved = -1;
        g_thread_exit (GINT_TO_POINTER (-1));
    }
#endif

    resolved = 1;
    return (GINT_TO_POINTER (1));
}

void client_connected (void)
{
    connected = 1;
    client_inmessage ("connect");
}

void client_disconnect (void)
{
    if (connected)
    {
      client_inmessage ("disconnect");
      g_source_destroy (g_main_context_find_source_by_id (NULL, source));
      g_io_channel_shutdown (io_channel, TRUE, NULL);
      g_io_channel_unref (io_channel);
      shutdown (sock, 2);
      close (sock);
      connected = 0;
    }
}


/* some other useful functions */

static gboolean
io_channel_cb (GIOChannel *source, GIOCondition condition)
{
  gchar *buf;
  
  source = source; /* get rid of the warnings */
  
  switch (condition)
  {
    case G_IO_IN :
    {
      if (client_readmsg (&buf) < 0)
      {
        g_warning ("client_readmsg failed, aborting connection\n");
        client_disconnect ();
      }
      else
      {
        if (strlen (buf)) client_inmessage (buf);
        
        if (strncmp ("noconnecting", buf, 12) == 0)
        {
          connected = 1; /* so we can disconnect :) */
          client_disconnect ();
        }
        g_free (buf);
      }
    }; break;
    default : break;
  }
  
  return TRUE;
}

int client_sendmsg (char *str)
{
    char c = 0xFF;
    GError *error = NULL;
    
    g_io_channel_write_chars (io_channel, str, -1, NULL, &error);
    g_io_channel_write_chars (io_channel, &c, 1, NULL, &error);
    g_io_channel_flush (io_channel, &error);

#ifdef DEBUG
    printf ("> %s\n", str);
#endif

    return 0;
}

int client_readmsg (gchar **str)
{
    gint bytes = 0;
    gchar buf[1024];
    GError *error = NULL;
    gint i = 0;
  
    do
    {
      switch (g_io_channel_read_chars (io_channel, &buf[i], 1, &bytes, &error))
      {
        case G_IO_STATUS_EOF :
          g_warning ("End of file.");
          return -1;
          break;
        
        case G_IO_STATUS_AGAIN :
          g_warning ("Resource temporarily unavailable.");
          return -1;
          break;
        
        case G_IO_STATUS_ERROR :
          g_warning ("Error");
          return -1;
          break;
        
        case G_IO_STATUS_NORMAL :
          if (error != NULL)
          {
            g_warning ("ERROR READING: %s\n", error->message);
            g_error_free (error);
            return -1;
          }; break;
      }
      i++;
    } while ((bytes == 1) && (buf[i-1] != (gchar)0xFF) && (i<1024));
    buf[i-1] = 0;

#ifdef DEBUG
    printf ("< %s\n", buf);
#endif
    
    *str = g_strdup (buf);

    return 0;
}

void server_ip (unsigned char buf[4])
{
#ifdef USE_IPV6
    struct sockaddr_in6 sin;
    struct sockaddr_in *sin4;
#else
    struct sockaddr_in sin;
#endif
    int len = sizeof(sin);

    getpeername (sock, (struct sockaddr *)&sin, &len);
#ifdef USE_IPV6
    if (sin.sin6_family == AF_INET6) {
	memcpy (buf, ((char *) &sin.sin6_addr) + 12, 4);
    } else {
	sin4 = (struct sockaddr_in *) &sin;
	memcpy (buf, &sin4->sin_addr, 4);
   }
#else
    memcpy (buf, &sin.sin_addr, 4);
#endif
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
