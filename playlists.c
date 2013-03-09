#include "hemp.h"

gchar
*canonical_location (char *location)
{
    /* Allocates uri but doesn't free it */
    gchar *uri;
    char *tmp;
    struct stat st;

    if (!stat(location, &st)) {
        tmp = realpath(location, NULL);
        uri = g_filename_to_uri(tmp, NULL, NULL);
        free(tmp);
    } else {
        uri = g_strdup(location);
    }
    return(uri);
}

char 
*playlist_path (void)
{
    /* allocates result, doesn't free it */
    /* returns NULL on any problem */
    char *path;
    char *result;
    struct stat st;

    path = calloc(PATH_MAX + 1, sizeof(char));
    strncpy(path, getenv("HOME"), PATH_MAX);
    strncat(path, "/", 1);
    strncat(path, PLAYLIST_DIR, PATH_MAX - strlen(path) - 1);
    mkdir(path, 0755);
    if (!stat(path, &st)) {
        if(S_ISDIR(st.st_mode)) {
            strncat(path, "/", 1);
            strncat(path, PLAYLIST_FILE, PATH_MAX - strlen(path) - 1);
            result = strdup(path);
            free(path);
            if (result) {
                return (result);
            }
        }
    }
    return(NULL);
}

void
init_lists (void)
{
    FILE *fp = NULL;
    char *uri = NULL;
    ssize_t len;
    struct hemp_playlist_entry *e;
  
    hotlist = g_queue_new();
    coldlist = g_queue_new();

    entry_by_uri = g_hash_table_new(g_str_hash, g_str_equal);

    if (fp = fopen(playlist_path(), "r")) {
        while (1) {
            len = getline(&uri, &len, fp);
            if (len < 0) break;
            if (len < 1) next;
            uri[len - 1] = '\0';
            e = new_playlist_entry(uri);
            free(uri);
            uri = NULL;
            g_queue_push_tail (coldlist, e);
        }
    fclose(fp);  
    } 
}


void
write_playlist_entry (FILE *fp, GHashTable *done,
                      gchar *uri)
{
    static gint one = 1;
    if (g_hash_table_lookup(done, uri)) {
        return;
    } else {
        g_hash_table_insert(done, uri, &one);
        fprintf(fp, "%s\n", uri);
    }
}

int
write_playlist (void)
{
    FILE *fp = NULL;
    guint i;
    struct hemp_playlist_entry *e;
    GHashTable *done;
    
    if (fp = fopen(playlist_path(), "w")) {
        done = g_hash_table_new(g_str_hash, g_str_equal);

        for (i = 0; i < g_queue_get_length(hotlist); i++) {
            e = g_queue_peek_nth(hotlist, i);
            if (e->finish_count == e->start_count) { 
                write_playlist_entry(fp, done, e->uri);
            }
        }
       
        for (i = 0; i < g_queue_get_length(coldlist); i++) { 
            e = g_queue_peek_nth(coldlist, i);
            write_playlist_entry(fp, done, e->uri);
        }

        if (current) {
            write_playlist_entry(fp, done, current->uri);
        }

        for (i = 0; i < g_queue_get_length(hotlist); i++) {
            e = g_queue_peek_nth(hotlist, i);
            if (e->finish_count < e->start_count) { 
                write_playlist_entry(fp, done, e->uri);
            }
        }

        if((fflush(fp)==0) && (fsync(fileno(fp))!=-1)) {
            if (fclose(fp) == 0) {
                return(1);
            }
        } else {
            fclose(fp);
            return(0); 
        }
    } else {
        return(0);
    }
}

int
uri_is_directory (gchar *uri)
{
    gchar *filename;
    struct stat st;
    filename = g_filename_from_uri(uri, NULL, NULL);
    if (!stat(filename, &st)) {
        if(S_ISDIR(st.st_mode)) {
            return(1);
        }
    }
    return(0);
}


int
valid_playlist_entry (struct hemp_playlist_entry *entry)
{
    /*TODO: check the struct */
    if (entry) {
        return(1);
    } else {
        return(0);
    }
}

struct hemp_playlist_entry
*new_playlist_entry (char *location)
{
    /* allocates a struct hemp_playlist_entry */
    /* with a copy of the argument */

    struct hemp_playlist_entry *entry;
    gchar *canonical;
    
    canonical = canonical_location(location);
    entry = g_hash_table_lookup(entry_by_uri, canonical);

    if (entry) {
        entry->refcount++;
    } else {
        entry = g_malloc0(sizeof(struct hemp_playlist_entry));
        entry->uri = canonical;
        entry->last_played = 0;
        entry->refcount = 1;
        g_hash_table_insert(entry_by_uri, canonical, entry);
    }
    return(entry);
}

void
release_playlist_entry (struct hemp_playlist_entry *entry)
{
    entry->refcount--;
    if (entry->refcount == 0) {
        g_free(entry);
    }
}

void
previous (int state)
{
    struct hemp_playlist_entry *e; 
    e = g_queue_pop_tail(hotlist);
    if (valid_playlist_entry(e)) {
        gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);
        if (current) {
            g_queue_push_head(hotlist, current);
        }
        current = e;
        g_object_set (G_OBJECT (pipeline), "uri", current->uri, NULL);
        if (state == HEMP_STATE_PAUSED) {
            gst_element_set_state (GST_ELEMENT (pipeline),
                                   GST_STATE_PAUSED);
        } else {
            gst_element_set_state (GST_ELEMENT (pipeline),
                                   GST_STATE_PLAYING);
        }
    } 
}

struct hemp_playlist_entry
*next_from_hotlist (void)
{
    struct hemp_playlist_entry *e;

    e = g_queue_pop_head(hotlist);
    if (valid_playlist_entry(e)) {
        return (e);
    } else {
        g_error("nothing on the hotlist");
    }
}    

void
directory_contents_to_hotlist(struct hemp_playlist_entry *directory)
{
    /* frees the struct hemp_playlist_entry passed *
     * if it's no longer on any playlists          */
    gchar *path;
    DIR *dirp;
    struct dirent *entry;
    char buf[PATH_MAX];
    int pathlength;

    path = g_filename_from_uri(directory->uri, NULL, NULL);
    release_playlist_entry(directory);
    pathlength = strlen(path);
    g_assert (pathlength + 3 < PATH_MAX);

    dirp = opendir(path);
    if (dirp == NULL) return;

    while(1) {
        entry = readdir(dirp);
        if (entry == NULL) break;
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            strcpy(buf, path);
            strncat(buf, "/", 1);
            strncat(buf, entry->d_name, PATH_MAX - (pathlength + 3));
            g_queue_push_head(hotlist, new_playlist_entry(buf));
        }
    }
    closedir(dirp);
    return;
}


void
next (int state, int is_skip) {
    struct hemp_playlist_entry *e;
    static int back_count = 0;
    time_t now;

    time(&now);

    gst_element_set_state (GST_ELEMENT (pipeline),
                           GST_STATE_NULL);
    if (current) {
        current->last_played = now;
        g_queue_push_tail(hotlist, current);
    }

    e = g_queue_peek_head(hotlist);
    if (e && (e->last_played + NO_REPEAT_TIME < now)) {
        current = g_queue_pop_head(hotlist);
    } else if (!g_queue_is_empty(coldlist)) {
        do {
            current = g_queue_pop_head(coldlist);
        } while (current->last_played > 0);
        /* Need to do it this way in case a coldlist entry 
         * is also on the hotlist and has already been played */
    } else if (e && is_skip) {
        current = g_queue_pop_head(hotlist); /* Never skip to silence. */
    } else {
        drop_pipeline(0);
        return;
    }

    current->start_count++;
    g_object_set (G_OBJECT (pipeline), "uri", current->uri, NULL);
    if (state == HEMP_STATE_PAUSED) {
        gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PAUSED);
    } else {
        gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
    }
}
