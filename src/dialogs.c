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

#ifdef HAVE_CONFIG_H
# include "../config.h"
#endif

#include <gtk/gtk.h>
#include <gnome.h>
#include <sys/types.h>
#include <dirent.h>

#include "gtetrinet.h"
#include "config.h"
#include "client.h"
#include "tetrinet.h"
#include "tetris.h"
#include "fields.h"
#include "misc.h"
#include "keys.h"
#include "sound.h"

/*****************************************************/
/* connecting dialog - a dialog with a cancel button */
/*****************************************************/
static GtkWidget *connectingdialog = 0;
static GtkWidget *progressbar;
static gint timeouttag = 0;

void connectingdialog_button (GnomeDialog *dialog, gint button, gpointer data)
{
    switch (button) {
    case 0:
        gtk_timeout_remove (timeouttag);
        timeouttag = 0;
        if (connectingdialog == 0) return;
        client_connectcancel ();
        gtk_widget_destroy (connectingdialog);
        connectingdialog = 0;
        break;
    }
}

gint connectingdialog_delete (GtkWidget *widget, gpointer data)
{
    return TRUE; /* dont kill me */
}

gint connectingdialog_timeout (gpointer data)
{
    GtkAdjustment *adj;
    adj = GTK_PROGRESS(progressbar)->adjustment;
    gtk_progress_set_value (GTK_PROGRESS(progressbar),
                            (adj->value+1)>adj->upper ?
                            adj->lower : adj->value+1);
    return TRUE;
}

void connectingdialog_new (void)
{
    if (connectingdialog) return;
    connectingdialog = gnome_dialog_new (_("Connect to server"),
                                         GNOME_STOCK_BUTTON_CANCEL,
                                         NULL);
    progressbar = gtk_progress_bar_new ();
    gtk_progress_set_activity_mode (GTK_PROGRESS(progressbar),
                                    TRUE);
    gtk_widget_show (progressbar);
    gtk_box_pack_start (GTK_BOX(GNOME_DIALOG(connectingdialog)->vbox),
                        progressbar, TRUE, TRUE, 0);

    timeouttag = gtk_timeout_add (20, (GtkFunction)connectingdialog_timeout,
                                  NULL);
    gtk_signal_connect (GTK_OBJECT(connectingdialog), "clicked",
                        GTK_SIGNAL_FUNC(connectingdialog_button), NULL);
    gtk_signal_connect (GTK_OBJECT(connectingdialog), "delete_event",
                        GTK_SIGNAL_FUNC(connectingdialog_delete), NULL);
    gtk_widget_show (connectingdialog);
}

void connectingdialog_destroy (void)
{
    gtk_timeout_remove (timeouttag);
    timeouttag = 0;
    if (connectingdialog == 0) return;
    gtk_widget_destroy (connectingdialog);
    connectingdialog = 0;
}

/*******************/
/* the team dialog */
/*******************/
void teamdialog_button (GnomeDialog *dialog, gint button, gpointer data)
{
    GtkWidget *entry = GTK_WIDGET(data);
    gchar *aux;
    switch (button) {
    case 0:
        aux = g_locale_from_utf8 (gtk_entry_get_text(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(entry)))), -1, NULL, NULL, NULL);
        tetrinet_changeteam (aux);
        gtk_widget_destroy (GTK_WIDGET(dialog));
        g_free (aux);
        break;
    case 1:
        gtk_widget_destroy (GTK_WIDGET(dialog));
        break;
    }
}

void teamdialog_new (void)
{
    GtkWidget *dialog, *table, *widget, *entry;
    dialog = gnome_dialog_new (_("Change team"),
                               GNOME_STOCK_BUTTON_OK,
                               GNOME_STOCK_BUTTON_CANCEL,
                               NULL);
    gnome_dialog_set_default (GNOME_DIALOG(dialog), 0);
    table = gtk_table_new (1, 2, FALSE);
    gtk_table_set_col_spacings (GTK_TABLE(table), GNOME_PAD_SMALL);
    widget = gtk_label_new (_("Team name:"));
    gtk_widget_show (widget);
    gtk_table_attach_defaults (GTK_TABLE(table), widget, 0, 1, 0, 1);
    entry = gnome_entry_new ("Team");
    gtk_entry_set_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(entry))),
                        team);
    gtk_widget_show (entry);
    gtk_table_attach_defaults (GTK_TABLE(table), entry, 1, 2, 0, 1);
    gtk_widget_show (table);
    gtk_box_pack_start (GTK_BOX(GNOME_DIALOG(dialog)->vbox),
                        table, TRUE, TRUE, 0);
    /* pass the entry in the data pointer */
    gtk_signal_connect (GTK_OBJECT(dialog), "clicked",
                        GTK_SIGNAL_FUNC(teamdialog_button), (gpointer)entry);
    gtk_widget_show (dialog);
}

/**********************/
/* the connect dialog */
/**********************/
static int connecting;
static GtkWidget *serveraddressentry, *nicknameentry, *teamnameentry, *spectatorcheck, *passwordentry;
static GtkWidget *connectdialog, *passwordlabel, *teamnamelabel;
static GtkWidget *originalradio, *tetrifastradio;
static GSList *gametypegroup;
static int oldgamemode;

void connectdialog_button (GnomeDialog *dialog, gint button, gpointer data)
{
    gchar *team_utf8, *nick; /* intermediate buffer for recoding purposes */
    switch (button) {
    case 0:
        /* connect now */
        spectating = GTK_TOGGLE_BUTTON(spectatorcheck)->active ? TRUE : FALSE;
        GTET_O_STRCPY (specpassword, gtk_entry_get_text (GTK_ENTRY(passwordentry)));
        team_utf8 = g_locale_from_utf8 (gtk_entry_get_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(teamnameentry)))),
                                        -1, NULL, NULL, NULL);
        GTET_O_STRCPY (team, team_utf8);
        nick =   g_locale_from_utf8 (gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (nicknameentry)))),
                                     -1, NULL, NULL, NULL);
        client_init (gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (serveraddressentry)))), nick);
        g_free (team_utf8);
        g_free (nick);
        break;
    case 1:
        gamemode = oldgamemode;
        gtk_widget_destroy (connectdialog);
        break;
    }
}

void connectdialog_spectoggle (GtkWidget *widget, gpointer data)
{
    if (GTK_TOGGLE_BUTTON(widget)->active) {
        gtk_widget_set_sensitive (passwordentry, TRUE);
        gtk_widget_set_sensitive (passwordlabel, TRUE);
        gtk_widget_set_sensitive (teamnameentry, FALSE);
        gtk_widget_set_sensitive (teamnamelabel, FALSE);
    }
    else {
        gtk_widget_set_sensitive (passwordentry, FALSE);
        gtk_widget_set_sensitive (passwordlabel, FALSE);
        gtk_widget_set_sensitive (teamnameentry, TRUE);
        gtk_widget_set_sensitive (teamnamelabel, TRUE);
    }
}

void connectdialog_originaltoggle (GtkWidget *widget, gpointer data)
{
    if (GTK_TOGGLE_BUTTON(widget)-> active) {
        gamemode = ORIGINAL;
    }
}

void connectdialog_tetrifasttoggle (GtkWidget *widget, gpointer data)
{
    if (GTK_TOGGLE_BUTTON(widget)-> active) {
        gamemode = TETRIFAST;
    }
}

void connectdialog_connected (void)
{
    gtk_widget_destroy (connectdialog);
}

void connectdialog_destroy (GtkWidget *widget, gpointer data)
{
    connecting = FALSE;
}

void connectdialog_new (void)
{
    GtkWidget *widget, *table1, *table2, *frame;
    gchar *aux;
    /* check if dialog is already displayed */
    if (connecting) return;
    connecting = TRUE;

    /* save some stuff */
    oldgamemode = gamemode;

    /* make dialog that asks for address/nickname */
    connectdialog = gnome_dialog_new (_("Connect to server"),
                                      GNOME_STOCK_BUTTON_OK,
                                      GNOME_STOCK_BUTTON_CANCEL,
                                      NULL);
    gnome_dialog_set_default (GNOME_DIALOG(connectdialog), 0);
    gtk_signal_connect (GTK_OBJECT(connectdialog), "clicked",
                        GTK_SIGNAL_FUNC(connectdialog_button), NULL);

    /* main table */
    table1 = gtk_table_new (2, 2, FALSE);
    gtk_table_set_row_spacings (GTK_TABLE(table1), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table1), GNOME_PAD_SMALL);

    /* server address */
    table2 = gtk_table_new (2, 1, FALSE);

    serveraddressentry = gnome_entry_new ("Server");
    gtk_entry_set_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(serveraddressentry))),
                        server);
    gtk_widget_show (serveraddressentry);
    gtk_table_attach (GTK_TABLE(table2), serveraddressentry,
                      0, 1, 0, 1, GTK_FILL | GTK_EXPAND,
                      GTK_FILL | GTK_EXPAND, 0, 0);
    /* game type radio buttons */
    originalradio = gtk_radio_button_new_with_label (NULL, _("Original"));
    gametypegroup = gtk_radio_button_group (GTK_RADIO_BUTTON(originalradio));
    tetrifastradio = gtk_radio_button_new_with_label (gametypegroup, _("TetriFast"));
    switch (gamemode) {
    case ORIGINAL:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(originalradio), TRUE);
        break;
    case TETRIFAST:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tetrifastradio), TRUE);
        break;
    }
    gtk_widget_show (originalradio);
    gtk_widget_show (tetrifastradio);
    widget = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
    gtk_box_pack_start (GTK_BOX(widget), originalradio, 0, 0, 0);
    gtk_box_pack_start (GTK_BOX(widget), tetrifastradio, 0, 0, 0);
    gtk_widget_show (widget);
    gtk_table_attach (GTK_TABLE(table2), widget,
                      0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

    gtk_table_set_row_spacings (GTK_TABLE(table2), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table2), GNOME_PAD_SMALL);
    gtk_container_set_border_width (GTK_CONTAINER(table2), GNOME_PAD);
    gtk_widget_show (table2);
    frame = gtk_frame_new (_("Server address"));
    gtk_container_add (GTK_CONTAINER(frame), table2);
    gtk_widget_show (frame);
    gtk_table_attach (GTK_TABLE(table1), frame, 0, 2, 0, 1,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

    /* spectator checkbox + password */
    table2 = gtk_table_new (1, 1, FALSE);

    spectatorcheck = gtk_check_button_new_with_label (_("Connect as a spectator"));
    gtk_widget_show (spectatorcheck);
    gtk_table_attach (GTK_TABLE(table2), spectatorcheck, 0, 2, 0, 1,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
    passwordlabel = gtk_label_new (_("Password:"));
    gtk_widget_show (passwordlabel);
    gtk_table_attach (GTK_TABLE(table2), passwordlabel, 0, 1, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
    passwordentry = gtk_entry_new ();
    gtk_entry_set_visibility (GTK_ENTRY(passwordentry), FALSE);
    gtk_widget_show (passwordentry);
    gtk_table_attach (GTK_TABLE(table2), passwordentry, 1, 2, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

    gtk_table_set_row_spacings (GTK_TABLE(table2), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table2), GNOME_PAD_SMALL);
    gtk_container_set_border_width (GTK_CONTAINER(table2), GNOME_PAD);
    gtk_widget_show (table2);
    frame = gtk_frame_new (_("Spectate game"));
    gtk_container_add (GTK_CONTAINER(frame), table2);
    gtk_widget_show (frame);
    gtk_table_attach (GTK_TABLE(table1), frame, 0, 1, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

    /* nickname and teamname entries */
    table2 = gtk_table_new (1, 1, FALSE);

    widget = gtk_label_new (_("Nick name:"));
    gtk_widget_show (widget);
    gtk_table_attach (GTK_TABLE(table2), widget, 0, 1, 0, 1,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);
    nicknameentry = gnome_entry_new ("Nickname");
    aux = g_locale_to_utf8 (nick, -1, NULL, NULL, NULL);
    gtk_entry_set_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(nicknameentry))),
                        aux);
    g_free (aux);
    gtk_widget_show (nicknameentry);
    gtk_table_attach (GTK_TABLE(table2), nicknameentry, 1, 2, 0, 1,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);
    teamnamelabel = gtk_label_new (_("Team name:"));
    gtk_widget_show (teamnamelabel);
    gtk_table_attach (GTK_TABLE(table2), teamnamelabel, 0, 1, 1, 2,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);
    teamnameentry = gnome_entry_new ("Teamname");
    aux = g_locale_to_utf8 (team, -1, NULL, NULL, NULL);
    gtk_entry_set_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(teamnameentry))),
                        aux);
    g_free (aux);
    gtk_widget_show (teamnameentry);
    gtk_table_attach (GTK_TABLE(table2), teamnameentry, 1, 2, 1, 2,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);

    gtk_table_set_row_spacings (GTK_TABLE(table2), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table2), GNOME_PAD_SMALL);
    gtk_container_set_border_width (GTK_CONTAINER(table2), GNOME_PAD);
    gtk_widget_show (table2);
    frame = gtk_frame_new (_("Player information"));
    gtk_container_add (GTK_CONTAINER(frame), table2);
    gtk_widget_show (frame);
    gtk_table_attach (GTK_TABLE(table1), frame, 1, 2, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

    gtk_widget_show (table1);

    gtk_box_pack_start (GTK_BOX(GNOME_DIALOG(connectdialog)->vbox),
                        table1, TRUE, TRUE, 0);

    gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON(spectatorcheck), spectating);
    connectdialog_spectoggle (spectatorcheck, NULL);
    gtk_signal_connect (GTK_OBJECT(connectdialog), "destroy",
                        GTK_SIGNAL_FUNC(connectdialog_destroy), NULL);
    gtk_signal_connect (GTK_OBJECT(spectatorcheck), "toggled",
                        GTK_SIGNAL_FUNC(connectdialog_spectoggle), NULL);
    gtk_signal_connect (GTK_OBJECT(originalradio), "toggled",
                        GTK_SIGNAL_FUNC(connectdialog_originaltoggle), NULL);
    gtk_signal_connect (GTK_OBJECT(tetrifastradio), "toggled",
                        GTK_SIGNAL_FUNC(connectdialog_tetrifasttoggle), NULL);
    gtk_widget_show (connectdialog);
}

/*************************/
/* the change key dialog */
/*************************/
gint keydialog_key;

void key_dialog_callback (GtkWidget *widget, GdkEventKey *key)
{
    keydialog_key = key->keyval;
    gnome_dialog_close (GNOME_DIALOG(widget));
}

gint key_dialog (char *msg)
{
    GtkWidget *dialog, *label;

    dialog = gnome_dialog_new (_("Change Key"), GNOME_STOCK_BUTTON_CANCEL, NULL);
    label = gtk_label_new (msg);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX(GNOME_DIALOG(dialog)->vbox),
                        label, TRUE, TRUE, GNOME_PAD_SMALL);
    gnome_dialog_set_close (GNOME_DIALOG(dialog), TRUE);
    gtk_signal_connect (GTK_OBJECT(dialog), "key_press_event",
                        GTK_SIGNAL_FUNC(key_dialog_callback), NULL);
    gtk_widget_set_events (dialog, GDK_KEY_PRESS_MASK);
    keydialog_key = 0;
    gnome_dialog_run (GNOME_DIALOG(dialog));
    return keydialog_key;
}

/**************************/
/* the preferences dialog */
/**************************/
GtkWidget *prefdialog, *themelist, *keyclist;
GtkWidget *midientry, *miditable, *midicheck, *soundcheck;
GtkWidget *namelabel, *authlabel, *desclabel;

int themechanged, midichanged;

char *actions[K_NUM];
int actionid[K_NUM] = {
    K_RIGHT,
    K_LEFT,
    K_DOWN,
    K_ROTRIGHT,
    K_ROTLEFT,
    K_DROP,
    K_DISCARD,
    K_GAMEMSG
};

gint newkeys[K_NUM];

struct themelistentry {
    char dir[1024];
    char name[1024];
} themes[64];

int themecount;
int theme_select;

void prefdialog_check (GtkWidget *widget, gpointer data)
{
}

void prefdialog_drawkeys (void)
{
    char *array[2];
    int i;
    GtkTreeIter iter;
    GtkListStore *keys_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (keyclist)));

    actions[0] = _("Move right");
    actions[1] = _("Move left");
    actions[2] = _("Move down");
    actions[3] = _("Rotate right");
    actions[4] = _("Rotate left");
    actions[5] = _("Drop piece");
    actions[6] = _("Discard special");
    actions[7] = _("Send message");

    for (i = 0; i < K_NUM; i ++) {
        array[0] = actions[i];
        array[1] = keystr (newkeys[actionid[i]]);
        gtk_list_store_append (keys_store, &iter);
        gtk_list_store_set (keys_store, &iter,
                            0, actions[i],
                            1, keystr (newkeys[actionid[i]]),
                            2, i, -1);
    }
}

void prefdialog_clistupdate ()
{
    GtkTreeIter iter;
    GtkListStore *keys_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (keyclist)));
    gboolean valid;
    gint row = 0;
    
    valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (keys_store), &iter);
    while (valid)
    {
        gtk_list_store_set (keys_store, &iter, 1, keystr (newkeys[actionid[row]]), -1);
        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (keys_store), &iter);
        row ++;
    }
}

void prefdialog_restorekeys (GtkWidget *widget, gpointer data)
{
    int i;

    for (i = 0; i < K_NUM; i ++) newkeys[i] = defaultkeys[i];
    prefdialog_clistupdate ();
    gnome_property_box_changed (GNOME_PROPERTY_BOX(prefdialog));
}

void prefdialog_changekey (GtkWidget *widget, gpointer data)
{
    gchar buf[256], *key;
    gint k, row = 0;
    GtkListStore *keys_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (keyclist)));
    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (keyclist));
    GtkTreeIter pk_row;

    if (!gtk_tree_selection_get_selected (selection, NULL, &pk_row)) return;

    gtk_tree_model_get (GTK_TREE_MODEL (keys_store), &pk_row,
                        0, &key, 2, &row, -1);
    g_snprintf (buf, sizeof(buf), _("Press new key for \"%s\""), key);
    k = key_dialog (buf);
    if (k) {
        newkeys[actionid[row]] = k;
        prefdialog_clistupdate ();
        gnome_property_box_changed (GNOME_PROPERTY_BOX(prefdialog));
    }
}

void prefdialog_midion ()
{
    gtk_widget_set_sensitive (miditable, TRUE);
}

void prefdialog_midioff ()
{
    gtk_widget_set_sensitive (miditable, FALSE);
}

void prefdialog_soundon ()
{
    gtk_widget_set_sensitive (midicheck, TRUE);
    if (GTK_TOGGLE_BUTTON(midicheck)->active) {
        prefdialog_midion ();
    }
}

void prefdialog_soundoff ()
{
    gtk_widget_set_sensitive (midicheck, FALSE);
    prefdialog_midioff ();
}

void prefdialog_soundtoggle (GtkWidget *widget, gpointer data)
{
    if (GTK_TOGGLE_BUTTON(widget)->active) {
        prefdialog_soundon ();
    }
    else {
        prefdialog_soundoff ();
    }
    gnome_property_box_changed (GNOME_PROPERTY_BOX(prefdialog));
}

void prefdialog_miditoggle (GtkWidget *widget, gpointer data)
{
    if (GTK_TOGGLE_BUTTON(widget)->active) {
        prefdialog_midion ();
    }
    else {
        prefdialog_midioff ();
    }
    midichanged = TRUE;
    gnome_property_box_changed (GNOME_PROPERTY_BOX(prefdialog));
}


void prefdialog_midichanged (GtkWidget *widget, gpointer data)
{
    midichanged = TRUE;
    gnome_property_box_changed (GNOME_PROPERTY_BOX(prefdialog));
}

void prefdialog_restoremidi (GtkWidget *widget, gpointer data)
{
    gtk_entry_set_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(midientry))),
                        DEFAULTMIDICMD);
    gnome_property_box_changed (GNOME_PROPERTY_BOX(prefdialog));
}

void prefdialog_themelistselect (int n)
{
    char author[1024], desc[1024];

    config_getthemeinfo (themes[n].dir, NULL, author, desc);
    leftlabel_set (namelabel, themes[n].name);
    leftlabel_set (authlabel, author);
    leftlabel_set (desclabel, desc);
}

void prefdialog_themeselect (GtkTreeSelection *treeselection,
                             gpointer data)
{
    GtkListStore *model;
    GtkTreeIter iter;
    gint row;
  
    if (gtk_tree_selection_get_selected (treeselection, NULL, &iter))
    {
      model = GTK_LIST_STORE (gtk_tree_view_get_model (gtk_tree_selection_get_tree_view (treeselection)));
      gtk_tree_model_get (GTK_TREE_MODEL(model), &iter, 1, &row, -1);
      theme_select = row;
      themechanged = TRUE;
      prefdialog_themelistselect (row);
      gnome_property_box_changed (GNOME_PROPERTY_BOX(prefdialog));
    }
}

static int themelistcomp (const void *a1, const void *b1)
{
    const struct themelistentry *a = a1, *b = b1;
    return strcmp (a->name, b->name);
}

void prefdialog_themelist ()
{
    DIR *d;
    struct dirent *de;
    char buf[1024], str[1024], dir[1024];
    int i;
    char *basedir[2];
    GtkListStore *theme_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (themelist)));
    GtkTreeSelection *theme_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (themelist));
    GtkTreeIter iter, iter_selected;

    GTET_O_STRCPY (dir, getenv ("HOME"));
    GTET_O_STRCAT (dir, "/.gtetrinet/themes");

    basedir[0] = dir; /* load users themes first ... in case we run out */
    basedir[1] = GTETRINET_THEMES;

    themecount = 0;

    for (i = 0; i < 2; i ++) {
        d = opendir (basedir[i]);
        if (d) {
            while ((de = readdir(d))) {
                GTET_O_STRCPY (buf, basedir[i]);
                GTET_O_STRCAT (buf, "/");
                GTET_O_STRCAT (buf, de->d_name);
                GTET_O_STRCAT (buf, "/");

                if (config_getthemeinfo(buf, str, NULL, NULL) == 0) {
                    GTET_O_STRCPY (themes[themecount].dir, buf);
                    GTET_O_STRCPY (themes[themecount].name, str);
                    themecount ++;
                    if (themecount == (sizeof(themes) / sizeof(themes[0])))
                    { /* FIXME: should be dynamic */
                      g_warning("Too many theme files.\n");
                      closedir (d);
                      goto too_many_themes;
                    }
                }
            }
            closedir (d);
        }
    }
 too_many_themes:
    qsort (themes, themecount, sizeof(struct themelistentry), themelistcomp);

    theme_select = 0;
    gtk_list_store_clear (theme_store);
    for (i = 0; i < themecount; i ++) {
        char *text[2];
        text[0] = themes[i].name;
        text[1] = 0;
        gtk_list_store_append (theme_store, &iter);
        gtk_list_store_set (theme_store, &iter, 0, themes[i].name, 1, i, -1);
        if (strcmp(themes[i].dir, currenttheme) == 0)
        {
            iter_selected = iter;
            theme_select = i;
        }
    }
    gtk_tree_selection_select_iter (theme_selection, &iter_selected);
    prefdialog_themelistselect (theme_select);
}

void prefdialog_apply (GnomePropertyBox *dialog, gint pagenum)
{
    int i;
    
    if (pagenum == -1) {
        for (i = 0; i < K_NUM; i ++) {
            keys[i] = newkeys[i];
        }

        soundenable = GTK_TOGGLE_BUTTON(soundcheck)->active ? 1 : 0;
        midienable = GTK_TOGGLE_BUTTON(midicheck)->active ? 1 : 0;
        if (!soundenable && midienable) {
            midienable = 0;
            midichanged = TRUE;
        }

        if (themechanged) {
            GTET_O_STRCPY (currenttheme, themes[theme_select].dir);
            config_loadtheme (currenttheme);

            fields_page_destroy_contents ();
            fields_cleanup ();
            fields_init ();
            fields_page_new ();
            if (ingame) tetrinet_redrawfields ();
        }

        if (midichanged) {
            const char *midi = gtk_entry_get_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(midientry))));
            GTET_O_STRCPY (midicmd, midi);
        }

        if ((themechanged || midichanged) && ingame) {
            sound_stopmidi ();
            sound_playmidi (midifile);
        }

        themechanged = midichanged = FALSE;
    }
}

void prefdialog_new (void)
{
    GtkWidget *label, *table, *frame, *button, *button1, *widget, *table1, *divider;
    GtkListStore *theme_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    GtkListStore *keys_store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
    GtkTreeSelection *theme_selection, *keys_selection;
    int i;

    prefdialog = gnome_property_box_new();

    gtk_window_set_title(GTK_WINDOW(prefdialog), _("GTetrinet Preferences"));

    /* themes */
    themelist = gtk_tree_view_new_with_model (GTK_TREE_MODEL (theme_store));
    theme_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (themelist));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (themelist), FALSE);
    gtk_widget_set_usize (themelist, 160, 0);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (themelist), -1, _("Theme"), renderer,
                                                 "text", 0, NULL);
    gtk_widget_show (themelist);

    label = leftlabel_new (_("Select a theme from the list.\n"
                             "Install new themes in ~/.gtetrinet/themes/"));
    gtk_widget_show (label);

    table1 = gtk_table_new (3, 2, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER(table1), GNOME_PAD_SMALL);
    gtk_table_set_row_spacings (GTK_TABLE(table1), 0);
    gtk_table_set_col_spacings (GTK_TABLE(table1), GNOME_PAD_SMALL);
    widget = leftlabel_new (_("Name:"));
    gtk_widget_show (widget);
    gtk_table_attach (GTK_TABLE(table1), widget, 0, 1, 0, 1,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    widget = leftlabel_new (_("Author:"));
    gtk_widget_show (widget);
    gtk_table_attach (GTK_TABLE(table1), widget, 0, 1, 1, 2,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    widget = leftlabel_new (_("Description:"));
    gtk_widget_show (widget);
    gtk_table_attach (GTK_TABLE(table1), widget, 0, 1, 2, 3,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    namelabel = leftlabel_new ("");
    gtk_widget_show (namelabel);
    gtk_table_attach (GTK_TABLE(table1), namelabel, 1, 2, 0, 1,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    authlabel = leftlabel_new ("");
    gtk_widget_show (authlabel);
    gtk_table_attach (GTK_TABLE(table1), authlabel, 1, 2, 1, 2,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    desclabel = leftlabel_new ("");
    gtk_widget_show (desclabel);
    gtk_table_attach (GTK_TABLE(table1), desclabel, 1, 2, 2, 3,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show (table1);

    frame = gtk_frame_new (_("Selected Theme"));
    gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_container_set_border_width (GTK_CONTAINER(frame), GNOME_PAD_SMALL);
    gtk_widget_set_usize (frame, 240, 100);
    gtk_widget_show (frame);
    gtk_container_add (GTK_CONTAINER(frame), table1);

    table = gtk_table_new (2, 2, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER(table), GNOME_PAD);
    gtk_table_set_row_spacings (GTK_TABLE(table), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table), GNOME_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(table), themelist, 0, 1, 0, 2,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE(table), label, 1, 2, 0, 1,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE(table), frame, 1, 2, 1, 2,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_show (table);

    frame = gtk_frame_new (NULL);
    gtk_container_set_border_width (GTK_CONTAINER(frame), GNOME_PAD);
    gtk_container_add (GTK_CONTAINER(frame), table);
    gtk_widget_show (frame);
    label = gtk_label_new (_("Themes"));
    gtk_widget_show (label);
    gnome_property_box_append_page (GNOME_PROPERTY_BOX(prefdialog),
                                    frame, label);

    /* keyboard */
    keyclist = GTK_WIDGET (gtk_tree_view_new_with_model (GTK_TREE_MODEL(keys_store)));
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (keyclist), -1, _("Action"), renderer,
                                                 "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (keyclist), -1, _("Key"), renderer,
                                                 "text", 1, NULL);

    gtk_widget_set_usize (keyclist, 180, 0);
    gtk_widget_show (keyclist);

    label = gtk_label_new (_("Select an action from the list and press Change "
                             "Key to change the key associated with the action."));
    gtk_label_set_justify (GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap (GTK_LABEL(label), TRUE);
    gtk_widget_show (label);

    button = gtk_button_new_with_label (_("Change key..."));
    gtk_signal_connect (GTK_OBJECT(button), "clicked",
                        GTK_SIGNAL_FUNC (prefdialog_changekey), NULL);
    gtk_widget_show (button);

    button1 = gtk_button_new_with_label (_("Restore defaults"));
    gtk_signal_connect (GTK_OBJECT(button1), "clicked",
                        GTK_SIGNAL_FUNC (prefdialog_restorekeys), NULL);
    gtk_widget_show (button1);

    table = gtk_table_new (2, 2, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER(table), GNOME_PAD);
    gtk_table_set_row_spacings (GTK_TABLE(table), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table), GNOME_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(table), keyclist, 0, 1, 0, 2,
                      GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE(table), label, 1, 2, 0, 1,
                      GTK_FILL, 0, 0, 0);
    frame = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
    gtk_box_pack_end (GTK_BOX(frame), button1, FALSE, FALSE, 0);
    gtk_box_pack_end (GTK_BOX(frame), button, FALSE, FALSE, 0);
    gtk_widget_show (frame);
    gtk_table_attach (GTK_TABLE(table), frame, 1, 2, 1, 2,
                      GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_show (table);

    frame = gtk_frame_new (NULL);
    gtk_container_set_border_width (GTK_CONTAINER(frame), GNOME_PAD);
    gtk_container_add (GTK_CONTAINER(frame), table);
    gtk_widget_show (frame);
    label = gtk_label_new (_("Keyboard"));
    gtk_widget_show (label);
    gnome_property_box_append_page (GNOME_PROPERTY_BOX(prefdialog),
                                    frame, label);

    /* sound */
    soundcheck = gtk_check_button_new_with_label (_("Enable Sound"));
    gtk_widget_show (soundcheck);

    midicheck = gtk_check_button_new_with_label (_("Enable MIDI"));
    gtk_widget_show (midicheck);

    frame = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX(frame), soundcheck, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(frame), midicheck, FALSE, FALSE, 0);
    gtk_widget_show (frame);

    divider = gtk_hseparator_new ();
    gtk_widget_show (divider);

    midientry = gnome_entry_new ("MidiCmd");
    gtk_widget_show (midientry);

    widget = leftlabel_new (_("Enter command to play a midi file:"));
    gtk_widget_show (widget);

    label = gtk_label_new (_("The above command is run when a midi file is "
                             "to be played.  The name of the midi file is "
                             "placed in the environment variable MIDIFILE."));
    gtk_label_set_justify (GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap (GTK_LABEL(label), TRUE);
    gtk_widget_show (label);

    button = gtk_button_new_with_label (_("Restore defaults"));
    gtk_signal_connect (GTK_OBJECT(button), "clicked",
                        GTK_SIGNAL_FUNC (prefdialog_restoremidi), NULL);
    gtk_widget_show (button);

    miditable = gtk_table_new (4, 2, FALSE);
    gtk_table_set_row_spacings (GTK_TABLE(miditable), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(miditable), GNOME_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(miditable), widget, 0, 2, 0, 1,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE(miditable), midientry, 0, 2, 1, 2,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE(miditable), label, 0, 1, 2, 4,
                      GTK_FILL, GTK_FILL, GNOME_PAD_SMALL, GNOME_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(miditable), button, 1, 2, 2, 3,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_widget_show (miditable);

    table = gtk_table_new (3, 1, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER(table), GNOME_PAD);
    gtk_table_set_row_spacings (GTK_TABLE(table), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table), GNOME_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(table), frame, 0, 1, 0, 1,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_attach (GTK_TABLE(table), divider, 0, 1, 1, 2,
                      GTK_EXPAND | GTK_FILL, 0, 0, GNOME_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(table), miditable, 0, 1, 2, 3,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_show (table);

    frame = gtk_frame_new (NULL);
    gtk_container_set_border_width (GTK_CONTAINER(frame), GNOME_PAD);
    gtk_container_add (GTK_CONTAINER(frame), table);
    gtk_widget_show (frame);
    label = gtk_label_new (_("Sound"));
    gtk_widget_show (label);
    gnome_property_box_append_page (GNOME_PROPERTY_BOX(prefdialog),
                                    frame, label);

    /* init stuff */
    prefdialog_themelist ();

    gtk_entry_set_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(midientry))), midicmd);

    for (i = 0; i < K_NUM; i ++) newkeys[i] = keys[i];
    prefdialog_drawkeys ();

    gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON(soundcheck), soundenable);
    gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON(midicheck), midienable);

#ifdef HAVE_ESD
    if (midienable) prefdialog_midion ();
    else prefdialog_midioff ();
    if (soundenable) prefdialog_soundon ();
    else prefdialog_soundoff ();
#else
    prefdialog_soundoff ();
    gtk_widget_set_sensitive (soundcheck, FALSE);
#endif

    themechanged = midichanged = FALSE;

    g_signal_connect (G_OBJECT(soundcheck), "toggled",
                      GTK_SIGNAL_FUNC(prefdialog_soundtoggle), NULL);
    g_signal_connect (G_OBJECT(midicheck), "toggled",
                      GTK_SIGNAL_FUNC(prefdialog_miditoggle), NULL);
    g_signal_connect (G_OBJECT(gnome_entry_gtk_entry(GNOME_ENTRY(midientry))),
                      "changed", GTK_SIGNAL_FUNC(prefdialog_midichanged), NULL);
    g_signal_connect (G_OBJECT(theme_selection), "changed",
                      GTK_SIGNAL_FUNC (prefdialog_themeselect), NULL);
    g_signal_connect (G_OBJECT(prefdialog), "apply",
                      GTK_SIGNAL_FUNC(prefdialog_apply), NULL);
    g_signal_connect (G_OBJECT(GNOME_PROPERTY_BOX(prefdialog)->ok_button), "clicked",
                      GTK_SIGNAL_FUNC(prefdialog_check), NULL);
    g_signal_connect (G_OBJECT(GNOME_PROPERTY_BOX(prefdialog)->apply_button), "clicked",
                      GTK_SIGNAL_FUNC(prefdialog_check), NULL);
    gtk_widget_show (prefdialog);
}
