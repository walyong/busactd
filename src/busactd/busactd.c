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
#include <glib.h>
#include <gio/gio.h>
#include <libsystem/libsystem.h>
#include <libsystem/glib-util.h>

#include "busactd.h"
#include "log.h"

struct busactd_listener *busactd_listener_new(struct busactd *busactd) {
        struct busactd_listener *listener;

        listener = new0(struct busactd_listener, 1);
        if (!listener)
                return NULL;

        listener->busactd = busactd;
        listener->name_has_owner = NAME_HAS_OWNER_UNDECIDED;
        listener->ref_count++;
        listener->l_id = 0;

        return listener;
}

void busactd_listener_free(struct busactd_listener *listener) {

        if (!listener)
                return;

        g_list_free_full(listener->match_list, (GDestroyNotify) busactd_match_free);
        free(listener->busname);
        free(listener);
}

void busactd_listener_unref(struct busactd_listener *listener) {
        if (!listener)
                return;

        listener->ref_count--;
        if (listener->ref_count)
                return;

        busactd_listener_free(listener);
}

int busactd_listener_busname_compare_func(struct busactd_listener *listener, const char *busname) {

        assert(listener);
        assert(busname);

        return strcmp(listener->busname, busname);
}

struct busactd_listener *busactd_listener_get(struct busactd *busactd, const char *busname) {
        struct busactd_listener *listener;
        GList *list;

        list = g_list_find_custom(busactd->listener_list,
                                  busname,
                                  (GCompareFunc) busactd_listener_busname_compare_func);
        if (list) {
                listener = list->data;
                listener->ref_count++;
                return listener;
        }

        listener = busactd_listener_new(busactd);
        listener->busname = strdup(busname);
        if (!listener->busname) {
                busactd_listener_free(listener);
                return NULL;
        }

        return listener;
}

struct busactd_match *busactd_match_new(struct busactd_listener *listener) {
        struct busactd_match *match;

        assert(listener);

        match = new0(struct busactd_match, 1);
        if (!match)
                return NULL;

        match->listener = listener;

        return match;
}

void busactd_match_free(struct busactd_match *match) {

        if (!match)
                return;

        free(match->sender);
        free(match->path);
        free(match->interface);
        free(match->member);
        free(match->arg);
        free(match);
}

static int busactd_listener_update_name_has_owner(struct busactd_listener *listener) {
        struct busactd *busactd;
        g_autoptr(GError) error = NULL;
        g_autoptr(GVariant) gvar = NULL;
        bool has_owner;

        assert(listener);
        busactd = listener->busactd;
        assert(busactd);

        gvar = g_dbus_connection_call_sync(busactd->bus->connection,
                                           "org.freedesktop.DBus",
                                           "/org/freedesktop/DBus",
                                           "org.freedesktop.DBus",
                                           "NameHasOwner",
                                           g_variant_new("(s)",
                                                         listener->busname),
                                           NULL,
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,
                                           NULL,
                                           &error);

        g_variant_get(gvar, "(b)", &has_owner);

        listener->name_has_owner = has_owner ? NAME_HAS_OWNER_TRUE : NAME_HAS_OWNER_FALSE;

        return 0;
}

static void busactd_dbus_name_owner_changed_callback(
                GDBusConnection *connection,
                const gchar *sender_name,
                const gchar *object_path,
                const gchar *interface_name,
                const gchar *signal_name,
                GVariant *parameters,
                void *user_data) {

        _cleanup_free_ const char *arg_0 = NULL, *arg_1 = NULL, *arg_2 = NULL;
        struct busactd_listener *listener = user_data;

        assert(user_data);

        g_variant_get(parameters, "(sss)", &arg_0, &arg_1, &arg_2);

        /* If the name owner process is activated then: */
        /* arg_0: busname */
        /* arg_1: "" */
        /* arg_2: ":x.xxx" */

        /* If the name owner process is deactivated then: */
        /* arg_0: busname */
        /* arg_1: ":x.xxx" */
        /* arg_2: "" */

        assert(streq(listener->busname, arg_0));

        if (isempty(arg_2))
                listener->name_has_owner = NAME_HAS_OWNER_FALSE;
        else
                listener->name_has_owner = NAME_HAS_OWNER_TRUE;

        busactd_register_listener(listener);
}

static void busactd_dbus_subscribe_signal_callback(
                GDBusConnection *connection,
                const gchar *sender_name,
                const gchar *object_path,
                const gchar *interface_name,
                const gchar *signal_name,
                GVariant *parameters,
                void *userdata) {

        struct busactd_listener *listener = userdata;
        g_autoptr(GError) error = NULL;

        assert(listener);

        if (!g_dbus_connection_emit_signal(connection,
                                           listener->busname,
                                           object_path,
                                           interface_name,
                                           signal_name,
                                           parameters,
                                           &error)) {
                log_err("Failed to emit signal"
                        "(busname(%s), path(%s), interface(%s), signal(%s)): %s\n",
                        listener->busname, object_path, interface_name, signal_name, error->message);

                return;
        }

        log_dbg("emit signal:"
                "busname(%s), sender(%s), object(%s), interface(%s), signal(%s)",
                listener->busname, sender_name, object_path, interface_name, signal_name);
}

static void busactd_listener_subscribe_signal(struct busactd_listener *listener) {
        struct busactd *busactd;
        GList *list;

        assert(listener);
        busactd = listener->busactd;
        assert(busactd);

        FOREACH_G_LIST(list, listener->match_list) {
                struct busactd_match *match = list->data;

                if (match->m_id)
                        continue;

                match->m_id = g_dbus_connection_signal_subscribe(busactd->bus->connection,
                                                                 match->sender ? match->sender : NULL,
                                                                 match->interface ? match->interface : NULL,
                                                                 match->member ? match->member : NULL,
                                                                 match->path ? match->path : NULL,
                                                                 match->arg ? match->arg : NULL,
                                                                 G_DBUS_SIGNAL_FLAGS_NONE,
                                                                 busactd_dbus_subscribe_signal_callback,
                                                                 listener,
                                                                 NULL);
                if (!match->m_id)
                        log_dbg("Failed to subscribe signal:"
                                "sender(%s), path(%s), interface(%s), member(%s), arg(%s)",
                                match->sender, match->path, match->interface, match->member, match->arg);
                else
                        log_dbg("Start subscribe signal:"
                                "sender(%s), path(%s), interface(%s), member(%s), arg(%s)",
                                match->sender, match->path, match->interface, match->member, match->arg);
        }
}

static void busactd_listener_unsubscribe_signal(struct busactd_listener *listener) {
        struct busactd *busactd;
        GList *list;

        assert(listener);
        busactd = listener->busactd;
        assert(busactd);

        FOREACH_G_LIST(list, listener->match_list) {
                struct busactd_match *match = list->data;

                if (!match->m_id)
                        continue;

                log_dbg("Stop subscribe signal:"
                        "sender(%s), path(%s), interface(%s), member(%s), arg(%s)",
                        match->sender, match->path, match->interface, match->member, match->arg);

                g_dbus_connection_signal_unsubscribe(
                        busactd->bus->connection,
                        match->m_id);

                match->m_id = 0;
        }
}

void busactd_register_listener(struct busactd_listener *listener) {
        struct busactd *busactd;

        assert(listener);
        busactd = listener->busactd;
        assert(busactd);

        if (listener->name_has_owner == NAME_HAS_OWNER_UNDECIDED)
                busactd_listener_update_name_has_owner(listener);

        /* If already subscribe for this listener then do not
         * subscribe NameOwnerChanged signal more. */
        if (!listener->l_id)
                listener->l_id = g_dbus_connection_signal_subscribe(
                        busactd->bus->connection,                       // connection
                        "org.freedesktop.DBus",                         // sender name
                        "org.freedesktop.DBus",                         // interface name
                        "NameOwnerChanged",                             // signal name
                        "/org/freedesktop/DBus",                        // object path
                        listener->busname,                              // arguments
                        G_DBUS_SIGNAL_FLAGS_NONE,                       // flags
                        busactd_dbus_name_owner_changed_callback,       // callback function
                        listener,                                       // user data
                        NULL);                                          // function to free user data

        switch (listener->name_has_owner) {
        case NAME_HAS_OWNER_FALSE:
                busactd_listener_subscribe_signal(listener);
                break;
        case NAME_HAS_OWNER_TRUE:
                busactd_listener_unsubscribe_signal(listener);
                break;
        default:
                log_err("Should not be reached here!!!");
                assert(false);
                break;
        }
}

struct busactd_listener *busactd_add_listener(struct busactd_listener *listener) {
        struct busactd *busactd = listener->busactd;
        struct busactd_listener *l;
        GList *list;

        assert(listener);
        assert(listener->busname);

        list = g_list_find_custom(busactd->listener_list,
                                  listener->busname,
                                  (GCompareFunc) busactd_listener_busname_compare_func);
        if (!list) {
                busactd->listener_list = g_list_append(busactd->listener_list, listener);
                busactd_register_listener(listener);

                return listener;
        }

        l = list->data;

        assert(l);

        if (l != listener) {
                l->match_list = g_list_concat(l->match_list, listener->match_list);
                free(listener->busname);
                free(listener);
        }

        busactd_register_listener(l);

        return l;
}

void busactd_remove_listener(struct busactd_listener *listener) {
        struct busactd *busactd = listener->busactd;

        if (!listener)
                return;

        busactd->listener_list = g_list_remove(busactd->listener_list, listener);
        busactd_listener_free(listener);
}

int busactd_match_compare_func(struct busactd_match *a, struct busactd_match *b) {

        assert(a);
        assert(b);

        if (streq_ptr(a->sender, b->sender) &&
            streq_ptr(a->path, b->path) &&
            streq_ptr(a->interface, b->interface) &&
            streq_ptr(a->member, b->member) &&
            streq_ptr(a->arg, b->arg))
                return 0;

        return 1;
}

struct busactd_match *busactd_add_match(struct busactd_match *match) {
        struct busactd_listener *listener;
        struct busactd_match *m;
        GList *list;

        assert(match);
        listener = match->listener;
        assert(listener);

        list = g_list_find_custom(listener->match_list,
                                  match,
                                  (GCompareFunc) busactd_match_compare_func);
        if (!list) {
                listener->match_list = g_list_append(listener->match_list, match);
                busactd_add_listener(listener);
                return match;
        }

        m = list->data;

        assert(m);

        if (m != match)
                busactd_match_free(match);

        return m;
}

void busactd_remove_match(struct busactd_match *match) {
        struct busactd_listener *listener = match->listener;

        if (!match || match->type != BUSACTD_MATCH_TYPE_RUNTIME)
                return;

        assert(listener);

        listener->match_list = g_list_remove(listener->match_list, match);
        busactd_match_free(match);

        if (g_list_length(listener->match_list))
                return;

        busactd_remove_listener(listener);
}

struct busactd_match *busactd_find_match_by_id(struct busactd *busactd, unsigned int id) {
        GList *l_list;

        assert(busactd);

        if (!id)
                return NULL;

        FOREACH_G_LIST(l_list, busactd->listener_list) {
                struct busactd_listener *listener = l_list->data;
                GList *m_list;

                assert(listener);

                FOREACH_G_LIST(m_list, listener->match_list) {
                        struct busactd_match *match = m_list->data;

                        if (match->m_id == id)
                                return match;
                }
        }

        return NULL;
}

int busactd_match_new_from_string(struct busactd_listener *listener, const char *string, struct busactd_match **match) {
        struct busactd_match *m;
        char *word, *state;
        size_t len;

        assert(listener);
        assert(match);
        assert(string);

        m = busactd_match_new(listener);
        if (!m)
                goto on_error;

        FOREACH_WORD(word, len, string, state) {
                _cleanup_free_ char *t = NULL;
                char *val;
                size_t e;

                t = strndup(word, len);
                if (!t)
                        goto on_error;

                e = strcspn(t, "=");
                val = t + e + 1;
                if (isempty(val))
                        continue;

                if (strncaseeq(t, "sender", e)) {
                        m->sender = strdup_unquote(val, QUOTES);
                        if (!m->sender)
                                goto on_error;
                } else if (strncaseeq(t, "path", e)) {
                        m->path = strdup_unquote(val, QUOTES);
                        if (!m->path)
                                goto on_error;
                } else if (strncaseeq(t, "interface", e)) {
                        m->interface = strdup_unquote(val, QUOTES);
                        if (!m->interface)
                                goto on_error;
                } else if (strncaseeq(t, "member", e)) {
                        m->member = strdup_unquote(val, QUOTES);
                        if (!m->member)
                                goto on_error;
                } else if (strncaseeq(t, "arg", e)) {
                        m->arg = strdup_unquote(val, QUOTES);
                        if (!m->arg)
                                goto on_error;
                } else
                        log_dbg("Undefined signal property: %s", t);
        }

        *match = m;

        return 0;

on_error:
        log_err("Failed to allocate");

        busactd_match_free(m);

        return -ENOMEM;
}
