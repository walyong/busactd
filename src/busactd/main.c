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

#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <getopt.h>

#include <glib.h>
#include <gio/gio.h>

#include <libsystem/libsystem.h>
#include <libsystem/config-parser.h>
#include <libsystem/glib-util.h>

#include "busactd.h"
#include "dbus.h"
#include "log.h"

#define BUSACT_CONF_EXT ".conf"
#define BUSACTD_TIMEOUT_SEC     10

static struct busactd_dbus bd_bus;
static struct busactd _busactd = {
        .type   = BUSACTD_TYPE_SYSTEM,
        .bus    = &bd_bus,
        .config_dirs[BUSACTD_LOAD_PRESET]    = "/usr/lib/" BUSACTD,
        .config_dirs[BUSACTD_LOAD_SYSCONFIG] = "/etc/" BUSACTD,
};
static struct busactd *busactd = &_busactd;

static void busactd_show_help(void) {
        printf("Usage: busactd [OPTIONS...]\n");
        printf("       -u  --user    run busactd for session\n");
        printf("       -h  --help    show this help\n");
}

static int parse_argv(int argc, char *argv[]) {
        static const struct option options[] = {
                { "user",       no_argument,       NULL, 'u'    },
                { "help",       no_argument,       NULL, 'h'    },
                { NULL,         0,                 NULL, 0      }
        };

        int c;

        assert(argc >= 0);
        assert(argv);

        while ((c = getopt_long(argc, argv, "uh", options, NULL)) >= 0) {

                switch (c) {

                case 'u':
                        busactd->type = BUSACTD_TYPE_USER;
                        break;

                case 'h':
                        busactd_show_help();
                        exit(EXIT_SUCCESS);
                        break;

                case '?':
                        return -EINVAL;

                default:
                        log_err("Error: unknown option code '%c'.", c);
                        busactd_show_help();
                        exit(EXIT_FAILURE);
                        return -EINVAL;
                }
        }

        return 0;
}

static int busactd_prepare_runtime_dir(struct busactd *busactd) {

        assert(busactd);

        if (busactd->type == BUSACTD_TYPE_SYSTEM) {
                if (snprintf(busactd->config_dirs[BUSACTD_LOAD_RUNTIME],
                             PATH_MAX,
                             "%s/%s",
                             BUSACTD_RUNTIME_DIR,
                             "system") < 0)
                        return -ENOMEM;
        } else {
                if (snprintf(busactd->config_dirs[BUSACTD_LOAD_RUNTIME],
                             PATH_MAX,
                             "%s/%s/%s",
                             getenv("XDG_RUNTIME_DIR"),
                             BUSACTD,
                             "user") < 0)
                        return -ENOMEM;
        }

        return do_mkdir(busactd->config_dirs[BUSACTD_LOAD_RUNTIME], 0755);
}

static void busactd_unregister_listeners(GList *listeners) {

        if (!listeners)
                return;

        g_list_free_full(listeners, (GDestroyNotify)busactd_listener_free);
}

static int busactd_config_parse_dbus_signal(
                const char *filename,
                unsigned line,
                const char *section,
                const char *lvalue,
                int ltype,
                const char *rvalue,
                void *userdata) {

        struct busactd_listener *listener = userdata;
        struct busactd_match *match = NULL;
        int r;

        assert(filename);
        assert(lvalue);
        assert(rvalue);
        assert(userdata);

        if (isempty(rvalue)) {
                log_dbg("%s has no vaild data on %d, just skipped.", filename, line);
                return 0;
        }

        r = busactd_match_new_from_string(listener, rvalue, &match);
        if (r < 0)
                return r;

        match->type = BUSACTD_MATCH_TYPE_PERSISTENT;

        listener->match_list = g_list_append(listener->match_list, match);

        return 0;
}

static int busactd_get_busname_from_name(const char *path, char **busname) {
        char *name = NULL, *b = NULL;

        assert(path);
        assert(busname);

        name = basename(path);
        b = strndup(name, strlen(name) - strlen(BUSACT_CONF_EXT));
        if (!b)
                return -ENOMEM;

        *busname = b;

        return 0;
}

static int busactd_parse_config_file(const char *path, void *userdata) {

        struct busactd_listener *listener = busactd_listener_new(busactd);
        int r;

        ConfigTableItem items[] = {
                { "BusAct",     "BusName",      config_parse_string,            0,      &listener->busname      },
                { "BusAct",     "Subscribe",    busactd_config_parse_dbus_signal, 0,    listener                },
                { NULL,         NULL,           NULL,                           0,      NULL                    }
        };

        assert(path);

        if (!endswith(path, BUSACT_CONF_EXT)) {
                log_dbg("file '%s' has no '%s' extension.", path, BUSACT_CONF_EXT);
                return -EINVAL;
        }

        if (!listener)
                return -ENOMEM;

        r = config_parse(path, (void *)items);
        if (r < 0) {
                log_err("Failed to parse configuration file: %s", strerror(-r));
                goto on_error;
        }

        if (!listener->busname) {
                r = busactd_get_busname_from_name(path, &listener->busname);
                if (r < 0) {
                        log_err("Failed to get busname from file name: %s", strerror(-r));
                        goto on_error;
                }
        }

        if (!listener->match_list) {
                log_dbg("Nothing to subscribe signal: %s", path);
                goto on_error;
        }

        busactd_add_listener(listener);

        return 0;

on_error:
        busactd_listener_free(listener);
        return r;
}

static gboolean busactd_load_listeners(gpointer user_data) {
        struct busactd *busactd = user_data;
        int i;

        assert(user_data);

        if (!busactd->bus->connection)
                return G_SOURCE_CONTINUE;

        for (i = 0; i < BUSACTD_LOAD_MAX; i++) {
                _cleanup_free_ char *dir = NULL;

                if (isempty(busactd->config_dirs[i]))
                        continue;

                if (asprintf(&dir, "%s/%s",
                             busactd->config_dirs[i],
                             busactd->type == BUSACTD_TYPE_SYSTEM ? "system" : "user") < 0) {
                        log_err("Failed to config dir: %s", strerror(ENOMEM));
                        continue;
                }

                (void) config_parse_dir(dir, busactd_parse_config_file, &busactd->listener_list);
        }

        log_info("listeners loading finished!!");

        return G_SOURCE_REMOVE;
}

static gboolean busactd_idle_timeout_callback(gpointer user_data) {
        if (g_list_length(busactd->listener_list))
                return G_SOURCE_CONTINUE;

        log_info("No listeners, mainloop quitting.");
        g_main_loop_quit(busactd->loop);

        return G_SOURCE_REMOVE;
}

static GSource *busactd_add_timeout_sec(guint sec,
                                        GSourceFunc func,
                                        gpointer data) {

        GSource *src = g_timeout_source_new_seconds(sec);

        g_assert(func);

        g_source_set_callback(src, func, data, NULL);
        g_source_attach(src, NULL);

        return src;
}

int main(int argc, char *argv[]) {
        int r;

        log_dbg("Running busact daemon...");

        r = parse_argv(argc, argv);
        if (r < 0)
                goto finish;

        r = busactd_prepare_runtime_dir(busactd);
        if (r < 0)
                goto finish;

        r = busactd_dbus_initialize(busactd);
        if (r < 0)
                goto finish;

        g_idle_add(busactd_load_listeners, busactd);

        busactd->idle_timeout_source = busactd_add_timeout_sec(BUSACTD_TIMEOUT_SEC,
                                                               busactd_idle_timeout_callback,
                                                               NULL);

        busactd->loop = g_main_loop_new(NULL, FALSE);
        log_dbg("Enter to main loop...");
        g_main_loop_run(busactd->loop);

        busactd_unregister_listeners(busactd->listener_list);

finish:
        log_dbg("Stop busact daemon...");

        return r < 0 ? EXIT_FAILURE: EXIT_SUCCESS;
}
