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
#include <errno.h>
#include <assert.h>

#include <glib.h>
#include <gio/gio.h>

#include <libsystem/libsystem.h>
#include <libsystem/glib-util.h>

#include "busactd.h"
#include "dbus.h"
#include "log.h"

static const gchar busactd_introspection_xml[] =
        "<node>"
        "  <interface name='org.tizen.busactd'>"
        "    <method name='ListListeners'>"
        "      <arg type='a{sa{ua{sv}}}' name='return' direction='out'/>"
        "    </method>"
        "    <method name='AddSubscription'>"
        "      <arg type='s' name='BusName' direction='in'/>"
        "      <arg type='s' name='Subscribe' direction='in'/>"
        "      <arg type='u' name='SubcriptionID' direction='out'/>"
        "    </method>"
        "    <method name='RemoveSubscription'>"
        "      <arg type='u' name='SubcriptionID' direction='in'/>"
        "      <arg type='s' name='Result' direction='out'/>"
        "    </method>"
        "  </interface>"
        "</node>";

static void busactd_dbus_handle_method_call_list_listeners(
                GDBusConnection *connection,
                const char *sender,
                const char *object_path,
                const char *interface_name,
                const char *method_name,
                GVariant *parameters,
                GDBusMethodInvocation *invocation,
                void *user_data) {

        struct busactd *busactd = user_data;
        GVariantBuilder builder;
        GList *list;

        assert(connection);
        assert(sender);
        assert(object_path);
        assert(interface_name);
        assert(method_name);
        assert(parameters);
        assert(invocation);
        assert(user_data);

        g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sa{ua{sv}}}"));

        FOREACH_G_LIST(list, busactd->listener_list) {
                struct busactd_listener *listener = list->data;
                GVariantBuilder l_builder;
                GList *m_list;

                assert(list->data);

                if (!listener->match_list)
                        continue;

                g_variant_builder_init(&l_builder, G_VARIANT_TYPE("a{ua{sv}}"));

                FOREACH_G_LIST(m_list, listener->match_list) {
                        struct busactd_match *match = m_list->data;
                        GVariantBuilder m_builder;

                        assert(list->data);

                        g_variant_builder_init(&m_builder, G_VARIANT_TYPE_VARDICT);

                        if (match->sender)
                                g_variant_builder_add(&m_builder,
                                                      "{sv}",
                                                      "Sender",
                                                      g_variant_new_string(match->sender));

                        if (match->path)
                                g_variant_builder_add(&m_builder,
                                                      "{sv}",
                                                      "Path",
                                                      g_variant_new_string(match->path));

                        if (match->interface)
                                g_variant_builder_add(&m_builder,
                                                      "{sv}",
                                                      "Interface",
                                                      g_variant_new_string(match->interface));

                        if (match->member)
                                g_variant_builder_add(&m_builder,
                                                      "{sv}",
                                                      "Member",
                                                      g_variant_new_string(match->member));

                        if (match->arg)
                                g_variant_builder_add(&m_builder,
                                                      "{sv}",
                                                      "Arg",
                                                      g_variant_new_string(match->arg));

                        g_variant_builder_add(&m_builder,
                                              "{sv}",
                                              "Type",
                                              g_variant_new_string(match->type == BUSACTD_MATCH_TYPE_PERSISTENT ? "PERSISTENT" : "RUNTIME"));

                        g_variant_builder_add(&l_builder,
                                              "{u@a{sv}}",
                                              match->m_id,
                                              g_variant_builder_end(&m_builder));
                }

                g_variant_builder_add(&builder,
                                      "{s@a{ua{sv}}}",
                                      listener->busname,
                                      g_variant_builder_end(&l_builder));
        }

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(a{sa{ua{sv}}})",
                                                            &builder));
}

static void busactd_dbus_handle_method_call_add_subscription(
                GDBusConnection *connection,
                const char *sender,
                const char *object_path,
                const char *interface_name,
                const char *method_name,
                GVariant *parameters,
                GDBusMethodInvocation *invocation,
                void *user_data) {

        _cleanup_free_ char *subscription = NULL, *busname = NULL;
        struct busactd *busactd = user_data;
        struct busactd_listener *listener;
        struct busactd_match *match;
        int r;

        assert(connection);
        assert(sender);
        assert(object_path);
        assert(interface_name);
        assert(method_name);
        assert(parameters);
        assert(invocation);
        assert(user_data);

        g_variant_get(parameters, "(ss)", &busname, &subscription);
        if (!busname || !subscription) {
                g_dbus_method_invocation_return_error_literal(
                        invocation,
                        G_DBUS_ERROR,
                        G_DBUS_ERROR_NO_MEMORY,
                        "Failed to get busname or subscription from parameters.");
                return;
        }

        listener = busactd_listener_get(busactd, busname);
        if (!listener) {
                g_dbus_method_invocation_return_error_literal(
                        invocation,
                        G_DBUS_ERROR,
                        G_DBUS_ERROR_NO_MEMORY,
                        "Failed to get new listener.");
                return;
        }

        r = busactd_match_new_from_string(listener, subscription, &match);
        if (r < 0) {
                g_dbus_method_invocation_return_error_literal(
                        invocation,
                        G_DBUS_ERROR,
                        G_DBUS_ERROR_NO_MEMORY,
                        "Failed to get busname or subscription from parameters.");
                busactd_listener_unref(listener);
                return;
        }

        match->type = BUSACTD_MATCH_TYPE_RUNTIME;

        match = busactd_add_match(match);

        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(u)", match->m_id));
}

static void busactd_dbus_handle_method_call_remove_subscription(
                GDBusConnection *connection,
                const char *sender,
                const char *object_path,
                const char *interface_name,
                const char *method_name,
                GVariant *parameters,
                GDBusMethodInvocation *invocation,
                void *user_data) {

        struct busactd *busactd = user_data;
        struct busactd_match *match;
        unsigned int id;

        assert(connection);
        assert(sender);
        assert(object_path);
        assert(interface_name);
        assert(method_name);
        assert(parameters);
        assert(invocation);
        assert(user_data);

        g_variant_get(parameters, "(u)", &id);
        if (!id) {
                g_dbus_method_invocation_return_error_literal(
                        invocation,
                        G_DBUS_ERROR,
                        G_DBUS_ERROR_INVALID_ARGS,
                        "Subscription ID has to be none 0.");
                return;
        }

        match = busactd_find_match_by_id(busactd, id);
        if (!match) {
                g_dbus_method_invocation_return_value(invocation,
                                                      g_variant_new("(s)", "not found"));
                return;
        }

        if (match->type != BUSACTD_MATCH_TYPE_RUNTIME) {
                g_dbus_method_invocation_return_value(invocation,
                                                      g_variant_new("(s)", "not allowed: match type is not RUNTIME"));
                return;
        }

        busactd_remove_match(match);
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(s)", "removed"));
}

static void busactd_dbus_handle_method_call(
                GDBusConnection *connection,
                const char *sender,
                const char *object_path,
                const char *interface_name,
                const char *method_name,
                GVariant *parameters,
                GDBusMethodInvocation *invocation,
                void *user_data) {

        assert(connection);
        assert(sender);
        assert(object_path);
        assert(interface_name);
        assert(method_name);
        assert(parameters);
        assert(invocation);
        assert(user_data);

        if (streq(method_name, "ListListeners"))
                busactd_dbus_handle_method_call_list_listeners(connection,
                                                               sender,
                                                               object_path,
                                                               interface_name,
                                                               method_name,
                                                               parameters,
                                                               invocation,
                                                               user_data);
        else if (streq(method_name, "AddSubscription"))
                busactd_dbus_handle_method_call_add_subscription(connection,
                                                                 sender,
                                                                 object_path,
                                                                 interface_name,
                                                                 method_name,
                                                                 parameters,
                                                                 invocation,
                                                                 user_data);
        else if (streq(method_name, "RemoveSubscription"))
                busactd_dbus_handle_method_call_remove_subscription(connection,
                                                                    sender,
                                                                    object_path,
                                                                    interface_name,
                                                                    method_name,
                                                                    parameters,
                                                                    invocation,
                                                                    user_data);
        else
                g_dbus_method_invocation_return_error(invocation,
                                                      G_DBUS_ERROR,
                                                      G_DBUS_ERROR_UNKNOWN_METHOD,
                                                      "Unknown method: %s", method_name);
}

static const GDBusInterfaceVTable busactd_dbus_interface_vtable = {
        .method_call = busactd_dbus_handle_method_call,
        .get_property = NULL,
        .set_property = NULL,
};

static void busactd_dbus_on_bus_accquired(GDBusConnection *connection,
                                          const gchar *name,
                                          gpointer user_data) {

        struct busactd *busactd = user_data;
        g_autoptr(GError) error = NULL;

        assert(connection);
        assert(name);
        assert(user_data);

        busactd->bus->connection = connection;

        busactd->bus->node_info = g_dbus_node_info_new_for_xml(busactd_introspection_xml, &error);
        if (error) {
                log_err("Failed to set node info: %s", error->message);
                assert(false);
        }

        g_dbus_connection_register_object(connection,
                                          "/Org/Tizen/BusActD",
                                          busactd->bus->node_info->interfaces[0],
                                          &busactd_dbus_interface_vtable,
                                          busactd,
                                          NULL,
                                          &error);
        if (error) {
                log_err("Failed to register objects: %s", error->message);
                assert(false);
        }
}

int busactd_dbus_initialize(void *busactd_data) {
        struct busactd *busactd = busactd_data;

        assert(busactd);
        assert(busactd->bus);

        busactd->bus->own_id = g_bus_own_name(busactd->type == BUSACTD_TYPE_SYSTEM ? G_BUS_TYPE_SYSTEM : G_BUS_TYPE_SESSION,
                                              "org.tizen.busactd",
                                              G_BUS_NAME_OWNER_FLAGS_NONE,
                                              busactd_dbus_on_bus_accquired,
                                              NULL,
                                              NULL,
                                              busactd,
                                              NULL);

        assert(busactd->bus->own_id);

        return 0;
}
