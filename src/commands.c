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
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "gtetrinet.h"
#include "client.h"
#include "tetrinet.h"
#include "partyline.h"
#include "misc.h"
#include "commands.h"
#include "dialogs.h"

#include "images/team24.xpm"

/*
 * GtkAction/GtkActionGroup/GtkUIManager/GtkToolbar and stock icons were
 * removed in GTK4.  Actions live in a GSimpleActionGroup attached to the
 * main window under the "win" prefix.  Menus use GMenuModel and the old
 * toolbar is represented by an ordinary GtkBox containing GtkButtons.
 */

static GSimpleActionGroup *action_group;
static GtkWindow *main_window;
static GMenu *game_menu;

static GtkWidget *connect_button;
static GtkWidget *disconnect_button;
static GtkWidget *start_button;
static GtkWidget *end_button;

static gboolean connect_visible = TRUE;
static gboolean disconnect_visible = FALSE;
static gboolean start_visible = TRUE;
static gboolean end_visible = FALSE;

/* Existing public command functions keep their old void(void) API. */
static void action_connect (GSimpleAction *action, GVariant *parameter, gpointer data);
static void action_disconnect (GSimpleAction *action, GVariant *parameter, gpointer data);
static void action_team (GSimpleAction *action, GVariant *parameter, gpointer data);
static void action_start (GSimpleAction *action, GVariant *parameter, gpointer data);
static void action_pause (GSimpleAction *action, GVariant *parameter, gpointer data);
static void action_end (GSimpleAction *action, GVariant *parameter, gpointer data);
#ifdef ENABLE_DETACH
static void action_detach (GSimpleAction *action, GVariant *parameter, gpointer data);
#endif
static void action_quit (GSimpleAction *action, GVariant *parameter, gpointer data);
static void action_preferences (GSimpleAction *action, GVariant *parameter, gpointer data);
static void action_about (GSimpleAction *action, GVariant *parameter, gpointer data);

static const GActionEntry entries[] = {
  { "connect",     action_connect,     NULL, NULL, NULL },
  { "disconnect",  action_disconnect,  NULL, NULL, NULL },
  { "change-team", action_team,        NULL, NULL, NULL },
  { "start-game",  action_start,       NULL, NULL, NULL },
  { "pause-game",  action_pause,       NULL, NULL, NULL },
  { "end-game",    action_end,         NULL, NULL, NULL },
#ifdef ENABLE_DETACH
  { "detach-page", action_detach,      NULL, NULL, NULL },
#endif
  { "quit",        action_quit,        NULL, NULL, NULL },
  { "preferences", action_preferences, NULL, NULL, NULL },
  { "about",       action_about,       NULL, NULL, NULL },
};

static const char *
legacy_action_name (const char *name)
{
  if (strcmp (name, "Connect") == 0)
    return "connect";
  if (strcmp (name, "Disconnect") == 0)
    return "disconnect";
  if (strcmp (name, "ChangeTeam") == 0)
    return "change-team";
  if (strcmp (name, "StartGame") == 0)
    return "start-game";
  if (strcmp (name, "PauseGame") == 0)
    return "pause-game";
  if (strcmp (name, "EndGame") == 0)
    return "end-game";
#ifdef ENABLE_DETACH
  if (strcmp (name, "DetachPage") == 0)
    return "detach-page";
#endif
  if (strcmp (name, "Exit") == 0)
    return "quit";
  if (strcmp (name, "Preferences") == 0)
    return "preferences";
  if (strcmp (name, "About") == 0)
    return "about";

  return NULL;
}

static GSimpleAction *
lookup_action (const char *legacy_name)
{
  const char *name = legacy_action_name (legacy_name);
  GAction *action;

  if (action_group == NULL || name == NULL)
    return NULL;

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), name);
  return action != NULL ? G_SIMPLE_ACTION (action) : NULL;
}

static void
set_action_enabled (const char *name, gboolean enabled)
{
  GSimpleAction *action = lookup_action (name);

  if (action != NULL)
    g_simple_action_set_enabled (action, enabled);
}

static void rebuild_game_menu (void);

static void
set_action_visible (const char *name, gboolean visible)
{
  GtkWidget *button = NULL;

  if (strcmp (name, "Connect") == 0) {
    connect_visible = visible;
    button = connect_button;
  }
  else if (strcmp (name, "Disconnect") == 0) {
    disconnect_visible = visible;
    button = disconnect_button;
  }
  else if (strcmp (name, "StartGame") == 0) {
    start_visible = visible;
    button = start_button;
  }
  else if (strcmp (name, "EndGame") == 0) {
    end_visible = visible;
    button = end_button;
  }

  if (button != NULL)
    gtk_widget_set_visible (button, visible);

  /* The old GtkAction visibility affected every proxy, including menus. */
  if (game_menu != NULL)
    rebuild_game_menu ();
}

#define ACTION_SHOW(name)    set_action_visible ((name), TRUE)
#define ACTION_HIDE(name)    set_action_visible ((name), FALSE)
#define ACTION_ENABLE(name)  set_action_enabled ((name), TRUE)
#define ACTION_DISABLE(name) set_action_enabled ((name), FALSE)

static void
append_game_section (GMenu *menu, GMenu *section)
{
  if (g_menu_model_get_n_items (G_MENU_MODEL (section)) != 0)
    g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
}

static void
rebuild_game_menu (void)
{
  GMenu *section;

  if (game_menu == NULL)
    return;

  g_menu_remove_all (game_menu);

  section = g_menu_new ();
  if (connect_visible)
    g_menu_append (section, _("_Connect to server..."), "win.connect");
  if (disconnect_visible)
    g_menu_append (section, _("_Disconnect from server"), "win.disconnect");
  append_game_section (game_menu, section);
  g_object_unref (section);

  section = g_menu_new ();
  g_menu_append (section, _("Change _team..."), "win.change-team");
  append_game_section (game_menu, section);
  g_object_unref (section);

  section = g_menu_new ();
  if (start_visible)
    g_menu_append (section, _("_Start game"), "win.start-game");
  g_menu_append (section, _("_Pause game"), "win.pause-game");
  if (end_visible)
    g_menu_append (section, _("_End game"), "win.end-game");
  append_game_section (game_menu, section);
  g_object_unref (section);

#ifdef ENABLE_DETACH
  section = g_menu_new ();
  g_menu_append (section, _("Detac_h page..."), "win.detach-page");
  append_game_section (game_menu, section);
  g_object_unref (section);
#endif

  section = g_menu_new ();
  g_menu_append (section, _("_Quit"), "win.quit");
  append_game_section (game_menu, section);
  g_object_unref (section);
}

static GtkWidget *
make_toolbar_button (const char *label,
                     const char *icon_name,
                     const char *tooltip,
                     const char *action_name)
{
  GtkWidget *button;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *text;

  button = gtk_button_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

  if (icon_name != NULL) {
    image = gtk_image_new_from_icon_name (icon_name);
    gtk_box_append (GTK_BOX (box), image);
  }

  text = gtk_label_new (label);
  gtk_box_append (GTK_BOX (box), text);
  gtk_button_set_child (GTK_BUTTON (button), box);

  if (tooltip != NULL)
    gtk_widget_set_tooltip_text (button, tooltip);

  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), action_name);
  return button;
}

static GtkWidget *
make_team_button (void)
{
  GtkWidget *button;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *text;
  GdkPixbuf *pixbuf;
  GdkTexture *texture;

  button = gtk_button_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

  pixbuf = gdk_pixbuf_new_from_xpm_data ((const char **) team24_xpm);
  texture = gdk_texture_new_for_pixbuf (pixbuf);
  image = gtk_image_new_from_paintable (GDK_PAINTABLE (texture));
  g_object_unref (texture);
  g_object_unref (pixbuf);

  text = gtk_label_new (_("Change team"));
  gtk_box_append (GTK_BOX (box), image);
  gtk_box_append (GTK_BOX (box), text);
  gtk_button_set_child (GTK_BUTTON (button), box);
  gtk_widget_set_tooltip_text (button, _("Change your current team name"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "win.change-team");

  return button;
}

static GtkWidget *
create_toolbar (void)
{
  GtkWidget *toolbar;
  GtkWidget *separator;
  GtkWidget *button;

  toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_add_css_class (toolbar, "toolbar");

  connect_button = make_toolbar_button (_("Connect"),
                                        "network-server-symbolic",
                                        _("Connect to a server"),
                                        "win.connect");
  gtk_box_append (GTK_BOX (toolbar), connect_button);

  disconnect_button = make_toolbar_button (_("Disconnect"),
                                           "network-offline-symbolic",
                                           _("Disconnect from the current server"),
                                           "win.disconnect");
  gtk_box_append (GTK_BOX (toolbar), disconnect_button);

  separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_append (GTK_BOX (toolbar), separator);

  start_button = make_toolbar_button (_("Start game"),
                                      "media-playback-start-symbolic",
                                      _("Start a new game"),
                                      "win.start-game");
  gtk_box_append (GTK_BOX (toolbar), start_button);

  end_button = make_toolbar_button (_("End game"),
                                    "media-playback-stop-symbolic",
                                    _("End the current game"),
                                    "win.end-game");
  gtk_box_append (GTK_BOX (toolbar), end_button);

  button = make_toolbar_button (_("Pause game"),
                                "media-playback-pause-symbolic",
                                _("Pause the game"),
                                "win.pause-game");
  gtk_box_append (GTK_BOX (toolbar), button);

  separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_append (GTK_BOX (toolbar), separator);

  button = make_team_button ();
  gtk_box_append (GTK_BOX (toolbar), button);

#ifdef ENABLE_DETACH
  separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_box_append (GTK_BOX (toolbar), separator);

  button = make_toolbar_button (_("Detach page"),
                                "edit-cut-symbolic",
                                _("Detach the current notebook page"),
                                "win.detach-page");
  gtk_box_append (GTK_BOX (toolbar), button);
#endif

  gtk_widget_set_visible (connect_button, connect_visible);
  gtk_widget_set_visible (disconnect_button, disconnect_visible);
  gtk_widget_set_visible (start_button, start_visible);
  gtk_widget_set_visible (end_button, end_visible);

  return toolbar;
}

static GtkWidget *
create_menubar (void)
{
  GMenu *menubar_model;
  GMenu *settings_menu;
  GtkWidget *menubar;

  menubar_model = g_menu_new ();
  game_menu = g_menu_new ();
  settings_menu = g_menu_new ();

  rebuild_game_menu ();

  g_menu_append (settings_menu, _("_Preferences"), "win.preferences");
  g_menu_append (settings_menu, _("_About"), "win.about");

  g_menu_append_submenu (menubar_model, _("_Game"), G_MENU_MODEL (game_menu));
  g_menu_append_submenu (menubar_model, _("_Settings"), G_MENU_MODEL (settings_menu));

  menubar = gtk_popover_menu_bar_new_from_model (G_MENU_MODEL (menubar_model));

  g_object_unref (menubar_model);

  /* Keep game_menu alive because show/hide functions mutate it later. */
  return menubar;
}

static void
add_shortcut (GtkShortcutController *controller,
              const char *trigger,
              const char *action_name)
{
  GtkShortcutTrigger *shortcut_trigger;
  GtkShortcutAction *shortcut_action;
  GtkShortcut *shortcut;

  shortcut_trigger = gtk_shortcut_trigger_parse_string (trigger);
  shortcut_action = gtk_named_action_new (action_name);
  shortcut = gtk_shortcut_new (shortcut_trigger, shortcut_action);
  gtk_shortcut_controller_add_shortcut (controller, shortcut);
}

static void
install_shortcuts (GtkWindow *window)
{
  GtkShortcutController *controller;

  controller = GTK_SHORTCUT_CONTROLLER (gtk_shortcut_controller_new ());
  gtk_shortcut_controller_set_scope (controller, GTK_SHORTCUT_SCOPE_MANAGED);

  add_shortcut (controller, "<Control>C", "win.connect");
  add_shortcut (controller, "<Control>D", "win.disconnect");
  add_shortcut (controller, "<Control>T", "win.change-team");
  add_shortcut (controller, "<Control>N", "win.start-game");
  add_shortcut (controller, "<Control>P", "win.pause-game");
  add_shortcut (controller, "<Control>S", "win.end-game");
  add_shortcut (controller, "<Control>Q", "win.quit");

  gtk_widget_add_controller (GTK_WIDGET (window), GTK_EVENT_CONTROLLER (controller));
}

void
make_menus (GtkWindow *app)
{
  GtkWidget *vbox;
  GtkWidget *menubar;
  GtkWidget *toolbar;
  GtkWidget *main_widget;

  main_window = app;

  action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (action_group),
                                   entries, G_N_ELEMENTS (entries), app);
  gtk_widget_insert_action_group (GTK_WIDGET (app), "win",
                                  G_ACTION_GROUP (action_group));

  install_shortcuts (app);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  menubar = create_menubar ();
  gtk_box_append (GTK_BOX (vbox), menubar);

  toolbar = create_toolbar ();
  gtk_box_append (GTK_BOX (vbox), toolbar);

  main_widget = gtk_window_get_child (app);
  if (main_widget != NULL) {
    /* Keep the old child alive while it is temporarily unparented. */
    g_object_ref (main_widget);
    gtk_window_set_child (app, NULL);
    gtk_box_append (GTK_BOX (vbox), main_widget);
    g_object_unref (main_widget);
  }

  gtk_window_set_child (app, vbox);

  ACTION_HIDE ("EndGame");
  ACTION_HIDE ("Disconnect");
}

/* GAction callbacks.  Keep these separate so commands.h need not change. */

static void
action_connect (GSimpleAction *action G_GNUC_UNUSED,
                GVariant *parameter G_GNUC_UNUSED,
                gpointer data G_GNUC_UNUSED)
{
  connect_command ();
}

static void
action_disconnect (GSimpleAction *action G_GNUC_UNUSED,
                   GVariant *parameter G_GNUC_UNUSED,
                   gpointer data G_GNUC_UNUSED)
{
  disconnect_command ();
}

static void
action_team (GSimpleAction *action G_GNUC_UNUSED,
             GVariant *parameter G_GNUC_UNUSED,
             gpointer data G_GNUC_UNUSED)
{
  team_command ();
}

static void
action_start (GSimpleAction *action G_GNUC_UNUSED,
              GVariant *parameter G_GNUC_UNUSED,
              gpointer data G_GNUC_UNUSED)
{
  start_command ();
}

static void
action_pause (GSimpleAction *action G_GNUC_UNUSED,
              GVariant *parameter G_GNUC_UNUSED,
              gpointer data G_GNUC_UNUSED)
{
  pause_command ();
}

static void
action_end (GSimpleAction *action G_GNUC_UNUSED,
            GVariant *parameter G_GNUC_UNUSED,
            gpointer data G_GNUC_UNUSED)
{
  end_command ();
}

#ifdef ENABLE_DETACH
static void
action_detach (GSimpleAction *action G_GNUC_UNUSED,
               GVariant *parameter G_GNUC_UNUSED,
               gpointer data G_GNUC_UNUSED)
{
  detach_command ();
}
#endif

static void
action_quit (GSimpleAction *action G_GNUC_UNUSED,
             GVariant *parameter G_GNUC_UNUSED,
             gpointer data G_GNUC_UNUSED)
{
  destroymain ();
}

static void
action_preferences (GSimpleAction *action G_GNUC_UNUSED,
                    GVariant *parameter G_GNUC_UNUSED,
                    gpointer data G_GNUC_UNUSED)
{
  preferences_command ();
}

static void
action_about (GSimpleAction *action G_GNUC_UNUSED,
              GVariant *parameter G_GNUC_UNUSED,
              gpointer data G_GNUC_UNUSED)
{
  about_command ();
}

/* callbacks */

void
connect_command (void)
{
  connectdialog_new ();
}

void
disconnect_command (void)
{
  client_disconnect ();
}

void
team_command (void)
{
  teamdialog_new ();
}

#ifdef ENABLE_DETACH
void
detach_command (void)
{
  move_current_page_to_window ();
}
#endif

void
start_command (void)
{
  char buf[22];

  g_snprintf (buf, sizeof (buf), "%i %i", 1, playernum);
  client_outmessage (OUT_STARTGAME, buf);
}

void
show_connect_button (void)
{
  ACTION_HIDE ("Disconnect");
  ACTION_SHOW ("Connect");
}

void
show_disconnect_button (void)
{
  ACTION_HIDE ("Connect");
  ACTION_SHOW ("Disconnect");
}

void
show_stop_button (void)
{
  ACTION_HIDE ("StartGame");
  ACTION_SHOW ("EndGame");
}

void
show_start_button (void)
{
  ACTION_HIDE ("EndGame");
  ACTION_SHOW ("StartGame");
}

void
end_command (void)
{
  char buf[22];

  g_snprintf (buf, sizeof (buf), "%i %i", 0, playernum);
  client_outmessage (OUT_STARTGAME, buf);
}

void
pause_command (void)
{
  char buf[22];

  g_snprintf (buf, sizeof (buf), "%i %i", paused ? 0 : 1, playernum);
  client_outmessage (OUT_PAUSE, buf);
}

void
preferences_command (void)
{
  prefdialog_new ();
}

/* the following function enables/disables things */

void
commands_checkstate (void)
{
  if (connected) {
    ACTION_DISABLE ("Connect");
    ACTION_ENABLE ("Disconnect");
  }
  else {
    ACTION_ENABLE ("Connect");
    ACTION_DISABLE ("Disconnect");
  }

  if (moderator) {
    if (ingame) {
      ACTION_DISABLE ("StartGame");
      ACTION_ENABLE ("PauseGame");
      ACTION_ENABLE ("EndGame");
    }
    else {
      ACTION_ENABLE ("StartGame");
      ACTION_DISABLE ("PauseGame");
      ACTION_DISABLE ("EndGame");
    }
  }
  else {
    ACTION_DISABLE ("StartGame");
    ACTION_DISABLE ("PauseGame");
    ACTION_DISABLE ("EndGame");
  }

  if (ingame || spectating)
    ACTION_DISABLE ("ChangeTeam");
  else
    ACTION_ENABLE ("ChangeTeam");

  partyline_connectstatus (connected);

  if (ingame)
    partyline_status (_("Game in progress"));
  else if (connected) {
    char buf[256];
    GTET_O_STRCPY (buf, _("Connected to\n"));
    GTET_O_STRCAT (buf, server);
    partyline_status (buf);
  }
  else
    partyline_status (_("Not connected"));
}

void
about_command (void)
{
  GFile *logo_file;
  GdkTexture *logo = NULL;
  GError *error = NULL;

  const char *authors[] = {
    "Ka-shu Wong <kswong@zip.com.au>",
    "James Antill <james@and.org>",
    "Jordi Mallach <jordi@sindominio.net>",
    "Dani Carbonell <bocata@panete.net>",
    NULL
  };
  const char *documenters[] = {
    "Jordi Mallach <jordi@sindominio.net>",
    NULL
  };
  /* Translators: translate as your names & emails */
  const char *translators = _("translator-credits");

  logo_file = g_file_new_for_path (PIXMAPSDIR "/gtetrinet.png");
  logo = gdk_texture_new_from_file (logo_file, &error);
  g_object_unref (logo_file);

  if (error != NULL) {
    g_warning ("Could not load GTetrinet logo: %s", error->message);
    g_error_free (error);
  }

  gtk_show_about_dialog (main_window,
                         "program-name", APPNAME,
                         "version", APPVERSION,
                         "copyright", "Copyright \xc2\xa9 2004, 2005 Jordi Mallach, Dani Carbonell\nCopyright \xc2\xa9 1999, 2000, 2001, 2002, 2003 Ka-shu Wong",
                         "comments", _("A Tetrinet client for GNOME.\n"),
                         "authors", authors,
                         "documenters", documenters,
                         "translator-credits",
                           strcmp (translators, "translator-credits") != 0 ? translators : NULL,
                         "logo", logo,
                         "website", "http://gtetrinet.sf.net",
                         "website-label", "GTetrinet Home Page",
                         NULL);

  if (logo != NULL)
    g_object_unref (logo);
}
