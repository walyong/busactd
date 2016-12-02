/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/*
 * busactd
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>

#include <glib.h>
#include <gio/gio.h>
#include <glib-object.h>

#include <libsystem/libsystem.h>

#include "log.h"

GMainLoop *loop = NULL;

static void busact_dbus_subscribe_signal_callback(GDBusConnection *connection,
                          const gchar *sender_name,
                          const gchar *object_path,
                          const gchar *interface_name,
                          const gchar *signal_name,
                          GVariant *parameters,
                          void *userdata) {

        log_dbg("busact test callback: entered");

        g_main_loop_quit(loop);

        return;
}

int main(int argc, const char *argv[]) {
        GDBusConnection *connection = NULL;
        unsigned int owner_id, subscribe_id;
        GError *error = NULL;
        int r = 0, mode;

        log_dbg("Running test busact...");

        if (argc > 1 && streq(argv[1], "--user"))
                mode = 1;
        else
                mode = 0;

        connection = g_bus_get_sync(mode ? G_BUS_TYPE_SESSION : G_BUS_TYPE_SYSTEM,
                                    NULL,
                                    &error);
        if (connection == NULL) {
                log_err("error connecting to D-Bus system bus: %s", error->message);
                r = error->code;
                goto finish;
        }

        owner_id = g_bus_own_name_on_connection(connection,
                                                "org.tizen.busactd.test",
                                                G_BUS_NAME_OWNER_FLAGS_NONE,
                                                NULL,
                                                NULL,
                                                NULL,
                                                NULL);

        subscribe_id = g_dbus_connection_signal_subscribe(
                connection,                                     // connection
                NULL,                                           // sender name
                "org.tizen.busactd.test",                       // interface name
                "Hello",                                        // signal name
                "/Org/Tizen/BusActD/Test",                      // object path
                NULL,                                           // arguments
                G_DBUS_SIGNAL_FLAGS_NONE,                       // flags
                busact_dbus_subscribe_signal_callback,          // callback function
                NULL,                                           // user data
                NULL);                                          // function to free user data

        loop = g_main_loop_new(NULL, FALSE);
        g_main_loop_run(loop);

        log_dbg("Stopping test busact...");
        g_main_loop_unref(loop);
        g_dbus_connection_signal_unsubscribe(connection, subscribe_id);
        g_bus_unown_name(owner_id);
        g_dbus_connection_close(connection, NULL, NULL, NULL);

finish:
        if (error)
                g_error_free(error);

        return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
