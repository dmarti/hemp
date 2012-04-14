#include <error.h>
#include <glib.h>
#include <dbus/dbus.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <gst/gst.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define HEMP_DBUS_DESTINATION "org.zgp.hemp"
#define HEMP_DBUS_PATH "/org/zgp/hemp"
#define HEMP_DBUS_SIGNAL "HempRemote"

#define NO_REPEAT_TIME (8 * 60 * 60)

#define PLAYLIST_DIR ".hemp"
#define PLAYLIST_FILE "playlist"

#define HEMP_STATE_PING     0
#define HEMP_STATE_PAUSED   1
#define HEMP_STATE_PLAYING  2
#define HEMP_STATE_TOGGLE   3
#define HEMP_STATE_STOP     4
#define HEMP_STATE_SKIP     5
#define HEMP_STATE_PREVIOUS 6

#define HEMP_PLAYLIST_HEAD 0
#define HEMP_PLAYLIST_TAIL 1

struct hemp_playlist_entry
{
    gchar *uri;
    unsigned int refcount;
    unsigned int start_count;
    unsigned int finish_count;
    time_t last_played;
};

DBusConnection *session_bus;

GstElement *pipeline;

struct hemp_playlist_entry *current;

GHashTable *entry_by_uri;
GQueue *hotlist;
GQueue *coldlist;

/* hemp.c */
void init_pipeline();
void drop_pipeline(int sig);

/* dbus.c */
void add_filter_to_loop(GMainLoop *loop);
void send_dbus_message(int state, int count, char **locations);
void handle_dbus_message(DBusMessage *msg);
int setup_dbus();
static DBusHandlerResult signal_filter (DBusConnection *connection, DBusMessage *message, void *user_data);


/* playlist.c */
gchar *canonical_location(char *location);
int uri_is_directory (gchar *uri);
void init_lists(void);
struct hemp_playlist_entry *new_playlist_entry(char *location);
void release_playlist_entry(struct hemp_playlist_entry *entry);
struct hemp_playlist_entry *next_from_hotlist();
void directory_contents_to_hotlist(struct hemp_playlist_entry *directory);
void previous (int state);
void next (int state, int is_skip);
char *playlist_path(void);
int write_playlist(void);
