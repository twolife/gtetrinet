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
    connectingdialog = gnome_dialog_new (_("Connect to Server"),
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
    switch (button) {
    case 0:
        tetrinet_changeteam (gtk_entry_get_text(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(entry)))));
        gtk_widget_destroy (GTK_WIDGET(dialog));
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

void connectdialog_button (GnomeDialog *dialog, gint button, gpointer data)
{
    switch (button) {
    case 0:
        /* connect now */
        spectating = GTK_TOGGLE_BUTTON(spectatorcheck)->active ? TRUE : FALSE;
        strcpy (specpassword, gtk_entry_get_text (GTK_ENTRY(passwordentry)));
        strcpy (team, gtk_entry_get_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(teamnameentry)))));
        client_init (gtk_entry_get_text(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(serveraddressentry)))),
                     gtk_entry_get_text(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(nicknameentry)))));
        break;
    case 1:
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

void connectdialog_cancel (GtkWidget *widget, gpointer data)
{
    gtk_widget_destroy (connectdialog);
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
    /* check if dialog is already displayed */
    if (connecting) return;
    connecting = TRUE;
    /* make dialog that asks for address/nickname */
    connectdialog = gnome_dialog_new (_("Connect to Server"),
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
    table2 = gtk_table_new (1, 1, FALSE);

    serveraddressentry = gnome_entry_new ("Server");
    gtk_entry_set_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(serveraddressentry))),
                        server);
    gtk_widget_show (serveraddressentry);
    gtk_table_attach (GTK_TABLE(table2), serveraddressentry,
                      0, 1, 0, 1, GTK_FILL | GTK_EXPAND,
                      GTK_FILL | GTK_EXPAND, 0, 0);

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
    gtk_entry_set_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(nicknameentry))),
                        nick);
    gtk_widget_show (nicknameentry);
    gtk_table_attach (GTK_TABLE(table2), nicknameentry, 1, 2, 0, 1,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);
    teamnamelabel = gtk_label_new (_("Team name:"));
    gtk_widget_show (teamnamelabel);
    gtk_table_attach (GTK_TABLE(table2), teamnamelabel, 0, 1, 1, 2,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);
    teamnameentry = gnome_entry_new ("Teamname");
    gtk_entry_set_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(teamnameentry))),
                        team);
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

char *keytitles[] = {
    N_("Action"),
    N_("Key")
};

char *actions[K_NUM] = {
    N_("Move right"),
    N_("Move left"),
    N_("Move down"),
    N_("Rotate right"),
    N_("Rotate left"),
    N_("Drop piece"),
    N_("Send Message")
};

int actionid[K_NUM] = {
    K_RIGHT,
    K_LEFT,
    K_DOWN,
    K_ROTRIGHT,
    K_ROTLEFT,
    K_DROP,
    K_GAMEMSG
};

gint newkeys[K_NUM];
int pk_row;

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

    for (i = 0; i < K_NUM; i ++) {
        array[0] = actions[i];
        array[1] = keystr (newkeys[actionid[i]]);
        gtk_clist_append (GTK_CLIST(keyclist), array);
    }
}

void prefdialog_clistupdate (int row)
{
    gtk_clist_set_text (GTK_CLIST(keyclist), row, 1,
                        keystr (newkeys[actionid[row]]));
}

void prefdialog_restorekeys (GtkWidget *widget, gpointer data)
{
    int i;
    for (i = 0; i < K_NUM; i ++) newkeys[i] = defaultkeys[i];
    for (i = 0; i < K_NUM; i ++) prefdialog_clistupdate (i);
    gnome_property_box_changed (GNOME_PROPERTY_BOX(prefdialog));
}

void prefdialog_clistselect (GtkWidget *widget, gint row, gint column,
                             GdkEventButton *event, gpointer data)
{
    pk_row = row;
}

void prefdialog_clistunselect (GtkWidget *widget, gint row, gint column,
                               gpointer data)
{
    pk_row = -1;
}

void prefdialog_changekey (GtkWidget *widget, gpointer data)
{
    char buf[256];
    gint k;

    if (pk_row == -1) return;

    sprintf (buf, _("Press new key for %s"), actions[pk_row]);
    k = key_dialog (buf);
    if (k) {
        newkeys[actionid[pk_row]] = k;
        prefdialog_clistupdate (pk_row);
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

void prefdialog_themeselect (GtkWidget *widget, gint row, gint column,
                             GdkEventButton *event, gpointer data)
{
    theme_select = row;
    themechanged = TRUE;
    prefdialog_themelistselect (row);
    gnome_property_box_changed (GNOME_PROPERTY_BOX(prefdialog));
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

    strcpy (dir, getenv ("HOME"));
    strcat (dir, "/.gtetrinet/themes");

    basedir[0] = GTETRINET_THEMES;
    basedir[1] = dir;

    themecount = 0;

    for (i = 0; i < 2; i ++) {
        d = opendir (basedir[i]);
        if (d) {
            while ((de = readdir(d))) {
                strcpy (buf, basedir[i]);
                strcat (buf, "/");
                strcat (buf, de->d_name);
                strcat (buf, "/");

                if (config_getthemeinfo(buf, str, NULL, NULL) == 0) {
                    strcpy (themes[themecount].dir, buf);
                    strcpy (themes[themecount].name, str);
                    themecount ++;
                }
            }
            closedir (d);
        }
    }
    qsort (themes, themecount, sizeof(struct themelistentry), themelistcomp);

    theme_select = 0;
    gtk_clist_clear (GTK_CLIST(themelist));
    for (i = 0; i < themecount; i ++) {
        char *text[2];
        text[0] = themes[i].name;
        text[1] = 0;
        gtk_clist_append (GTK_CLIST(themelist), text);
        if (strcmp(themes[i].dir, currenttheme) == 0)
            theme_select = i;
    }
    gtk_clist_select_row (GTK_CLIST(themelist), theme_select, 0);
    prefdialog_themelistselect (theme_select);
}

void prefdialog_apply (GnomePropertyBox *dialog, gint pagenum)
{
    int i;
    char *midi;

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
            strcpy (currenttheme, themes[theme_select].dir);
            config_loadtheme (currenttheme);

            fields_page_destroy_contents ();
            fields_cleanup ();
            fields_init ();
            fields_page_new ();
            if (ingame) tetrinet_redrawfields ();
        }

        if (midichanged) {
            midi = gtk_entry_get_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(midientry))));
            strcpy (midicmd, midi);
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
    int i;

    prefdialog = gnome_property_box_new();

    gtk_window_set_title(GTK_WINDOW(prefdialog), _("GTetrinet Preferences"));

    /* themes */
    themelist = gtk_clist_new (1);
    gtk_clist_set_column_width (GTK_CLIST(themelist), 0, 160);
    gtk_clist_set_selection_mode (GTK_CLIST(themelist), GTK_SELECTION_SINGLE);
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
    keyclist = gtk_clist_new_with_titles (2, keytitles);
    gtk_clist_set_column_width (GTK_CLIST(keyclist), 0, 100);
    gtk_clist_set_column_width (GTK_CLIST(keyclist), 1, 80);
    gtk_clist_set_selection_mode (GTK_CLIST(keyclist), GTK_SELECTION_SINGLE);
    gtk_clist_column_titles_passive (GTK_CLIST(keyclist));
    gtk_signal_connect (GTK_OBJECT(keyclist), "select_row",
                        GTK_SIGNAL_FUNC (prefdialog_clistselect), NULL);
    gtk_signal_connect (GTK_OBJECT(keyclist), "unselect_row",
                        GTK_SIGNAL_FUNC (prefdialog_clistunselect), NULL);
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

    pk_row = -1;
    themechanged = midichanged = FALSE;

    gtk_signal_connect (GTK_OBJECT(soundcheck), "toggled",
                        prefdialog_soundtoggle, NULL);
    gtk_signal_connect (GTK_OBJECT(midicheck), "toggled",
                        prefdialog_miditoggle, NULL);
    gtk_signal_connect (GTK_OBJECT(gnome_entry_gtk_entry(GNOME_ENTRY(midientry))),
                        "changed", GTK_SIGNAL_FUNC(prefdialog_midichanged), NULL);
    gtk_signal_connect (GTK_OBJECT(themelist), "select_row",
                        GTK_SIGNAL_FUNC (prefdialog_themeselect), NULL);
    gtk_signal_connect (GTK_OBJECT(prefdialog), "apply",
                        GTK_SIGNAL_FUNC(prefdialog_apply), NULL);
    gtk_signal_connect (GTK_OBJECT(GNOME_PROPERTY_BOX(prefdialog)->ok_button), "clicked",
                        GTK_SIGNAL_FUNC(prefdialog_check), NULL);
    gtk_signal_connect (GTK_OBJECT(GNOME_PROPERTY_BOX(prefdialog)->apply_button), "clicked",
                        GTK_SIGNAL_FUNC(prefdialog_check), NULL);
    gtk_widget_show (prefdialog);
}
