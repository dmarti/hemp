/* hemp: HEadless Music Player
 * Copyright (C) 2009 Donald B. Marti Jr.
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as 
 *   published by the Free Software Foundation, either version 3 of the 
 *   License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hemp.h"

void
add_filter_to_loop(GMainLoop *loop)
{
    dbus_connection_add_filter (session_bus, signal_filter, loop, NULL);
}

void 
send_dbus_message(int state, int count, char *locations[])
{
    DBusMessage *msg;
    DBusMessageIter *iter;

    int i;

    msg = dbus_message_new_signal (HEMP_DBUS_PATH,
                                   HEMP_DBUS_DESTINATION,
                                   HEMP_DBUS_SIGNAL);
    dbus_message_set_destination(msg, HEMP_DBUS_DESTINATION);

    dbus_message_append_args(msg, DBUS_TYPE_INT32, &state,
                             DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
                             &locations, count,
                             DBUS_TYPE_INVALID);

    if (!dbus_connection_send (session_bus, msg, NULL))
                fprintf (stderr, "error sending message\n");
    dbus_message_unref (msg);
    dbus_connection_flush (session_bus);
}

static DBusHandlerResult
signal_filter (DBusConnection *connection, DBusMessage *message,
               void *user_data)
{
    GMainLoop *loop = user_data;

    if (dbus_message_is_signal (message, HEMP_DBUS_DESTINATION,
                                HEMP_DBUS_SIGNAL)) {
        handle_dbus_message(message);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void
handle_dbus_message(DBusMessage *msg)
{
    int state;
    char **new_uris;
    int uri_count;
    GstState pipeline_state;
    int hemp_state;
    int i;
    int new_head = 0; /* is there a new hotlist head?
                         If so, play means skip */

    if (!strncmp (HEMP_DBUS_DESTINATION, dbus_message_get_destination(msg),
                  strlen(HEMP_DBUS_DESTINATION))) {
        dbus_message_get_args (msg, NULL,
                               DBUS_TYPE_INT32, &state,
                               DBUS_TYPE_ARRAY, DBUS_TYPE_STRING,
                               &new_uris, &uri_count,
                               DBUS_TYPE_INVALID);
        for (i = 0; i<uri_count; i++) {
            g_queue_push_head(hotlist, 
                              new_playlist_entry(new_uris[i]));
            new_head = 1; 
        }
        if (uri_count) write_playlist();

        if (!GST_IS_ELEMENT(pipeline)) init_pipeline();
        gst_element_get_state(GST_ELEMENT (pipeline),
                              &pipeline_state,
                              NULL,
                              GST_CLOCK_TIME_NONE);
        if (pipeline_state == GST_STATE_PAUSED) {
            hemp_state = HEMP_STATE_PAUSED;
        } else {
            hemp_state = HEMP_STATE_PLAYING;
        }

        switch (state) {
            case HEMP_STATE_TOGGLE:
                if (pipeline_state == GST_STATE_PLAYING) {
                    gst_element_set_state (GST_ELEMENT (pipeline),
                                           GST_STATE_PAUSED);
                } else {
                    gst_element_set_state (GST_ELEMENT (pipeline),
                                           GST_STATE_PLAYING);
                }
                break;
            case HEMP_STATE_SKIP:
                next(hemp_state, 1);
                break;
            case HEMP_STATE_PREVIOUS:
                previous(hemp_state);
                break;
            case HEMP_STATE_PLAYING:
                if (new_head) {
                    next(HEMP_STATE_PLAYING, 0);
                } else {
                    gst_element_set_state (GST_ELEMENT (pipeline),
                                           GST_STATE_PLAYING);
                }
                break;
            case HEMP_STATE_PAUSED:
                gst_element_set_state (GST_ELEMENT (pipeline),
                                       GST_STATE_PAUSED);
                break;
            case HEMP_STATE_STOP:
                drop_pipeline(0);
                break;
            case HEMP_STATE_PING:
                printf("ping!\n");
                break;
            default:
                printf("Unknown state %d\n", state);
       }
    }
}

int
setup_dbus ()
{
    /* returns 0 if it can't become owner of the name
     * 1 if it can */

    DBusError error;

    dbus_error_init(&error);
    session_bus = dbus_bus_get (DBUS_BUS_SESSION, &error);
    if (!session_bus) {
        fprintf (stderr, "%s: %s\n",
                 error.name, error.message);
        exit (1);
    }

    if (dbus_bus_name_has_owner(session_bus, HEMP_DBUS_DESTINATION, &error)) {
        return(0);
    }

    dbus_bus_request_name (session_bus, HEMP_DBUS_DESTINATION, 0, &error);
    if (dbus_error_is_set (&error)) {
        fprintf (stderr, "%s: %s\n",
                 error.name, error.message);
        dbus_connection_close (session_bus);
        return(0);
    }

    return(1);
}
