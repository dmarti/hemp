#include "hemp.h"

static gboolean
bus_call (GstBus *bus,
          GstMessage *msg,
          gpointer user_data)
{
    switch (GST_MESSAGE_TYPE (msg))
    {
        case GST_MESSAGE_EOS:
            {
                current->finish_count++;
                next(HEMP_STATE_PLAYING, 0); 
                break;
            }
        case GST_MESSAGE_ERROR:
            {
                gchar *debug;
                GError *err;
                gst_message_parse_error (msg, &err, &debug);
                g_free (debug);
                printf ("error %s\n", err->message);
                g_error_free (err);
                /* TODO: parse m3u and pls files */
                if (uri_is_directory(current->uri))  {
                    directory_contents_to_hotlist(current);
                    current = NULL;
                }
                init_pipeline();
                next(HEMP_STATE_PLAYING, 0);
                break;
            }
        default:
            break;
    }

    return true;
}

void
init_pipeline ()
{
    GstBus *bus;
    GstElement *fakesink;

    fakesink = gst_element_factory_make ("fakesink", "fakesink");

    if(pipeline) drop_pipeline(0);
    pipeline = gst_element_factory_make ("playbin", "player");

    g_object_set(G_OBJECT(pipeline), "video-sink", fakesink, NULL);

    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_call, NULL);
    gst_object_unref (bus);
}

int
main_loop ()
{
    DBusMessage *msg;
    char *destination;
    GMainLoop *loop;

    loop = g_main_loop_new (NULL, FALSE);
    dbus_connection_setup_with_g_main (session_bus, NULL);
    dbus_bus_add_match (session_bus,
                        "type='signal',interface='HEMP_DBUS_DESTINATION'",
                        NULL);
    add_filter_to_loop(loop);
    init_pipeline();
    next(HEMP_STATE_PLAYING, 0);
    g_main_loop_run (loop);
}

void
go_bg()
{
    pid_t pid;

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(0);
    }
    signal(SIGTSTP,SIG_IGN);
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }
    freopen( "/dev/null", "r", stdin);
    freopen( "/dev/null", "w", stdout);
    freopen( "/dev/null", "w", stderr);
    return;
}

void 
drop_pipeline (int sig) {
    gst_element_set_state (GST_ELEMENT (pipeline),
                           GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (pipeline));
    if (sig && sig != SIGHUP) {
        exit(0);
    }
}

int
main (int argc,
      char *argv[])
{
    struct sigaction my_action;
    gst_init (&argc, &argv);
    struct hemp_playlist_entry *e;
    char **new_locations;
    static int new_state = HEMP_STATE_PLAYING;
    static int background = 1;
    static int todo = 0;
    int i;
    int c;
    int count;

    while (1) {
        static struct option long_options[] = {
           {"foreground", no_argument, &background, 0},
           {"fg",         no_argument, &background, 0},
           {"help",       no_argument, &todo, 1},
           {"play",       no_argument, &new_state, HEMP_STATE_PLAYING},
           {"pause",      no_argument, &new_state, HEMP_STATE_PAUSED},
           {"stop",       no_argument, &new_state, HEMP_STATE_STOP},
           {"skip",       no_argument, &new_state, HEMP_STATE_SKIP},
           {"next",       no_argument, &new_state, HEMP_STATE_SKIP},
           {"back",       no_argument, &new_state, HEMP_STATE_PREVIOUS},
           {"previous",   no_argument, &new_state, HEMP_STATE_PREVIOUS},
           {"toggle",     no_argument, &new_state, HEMP_STATE_TOGGLE},
           {"playpause",  no_argument, &new_state, HEMP_STATE_TOGGLE},
           {"ping",       no_argument, &new_state, HEMP_STATE_PING},
           {"verbose",    no_argument, &todo, 1},
           {"version",    no_argument, &todo, 1},
           {0, 0, 0, 0}
        };
        int option_index = 0;
        c = getopt_long_only (argc, argv, "",
                         long_options, &option_index);
        if (c == -1) break;
    } 
    if (todo) {
            error(1, 0,
                  "--help, --verbose, and --version not implemented yet.");
    }
    count = argc - optind;
    if(setup_dbus() && new_state != HEMP_STATE_STOP) { /* main player */
        if (new_state == HEMP_STATE_PING) {
            exit(1);
        }
        my_action.sa_handler = drop_pipeline; 
        my_action.sa_flags = SA_RESTART;
        sigaction (SIGHUP, &my_action, NULL);
        sigaction (SIGINT, &my_action, NULL);
        sigaction (SIGTERM, &my_action, NULL);

        init_lists();

        for (i = 0; i < count; i++) {
            e = new_playlist_entry(argv[i+optind]);
            g_queue_push_tail(hotlist, e);
        }
        write_playlist();
        if (background) go_bg();
        main_loop();
    } else { /* send a message to the main player and exit */
        if (!background) {
            error(1, 0, "Another copy running.");
        }
        new_locations = g_malloc(count * sizeof(char *));
        for (i = 0; i < count; i++) {
            new_locations[i] = canonical_location(argv[i+optind]);
        }
        send_dbus_message(new_state, count, new_locations);
        for (i = 0; i < count; i++) {
            free(new_locations[i]);
        }
        free(new_locations); 
    }
}
