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

#pragma once

#include <limits.h>
#include <glib.h>
#include <gio/gio.h>

#include "dbus.h"

#define BUSACTD                 "busactd"
#define BUSACTD_RUNTIME_DIR     "/run/" BUSACTD

enum busactd_type {
        BUSACTD_TYPE_SYSTEM,
        BUSACTD_TYPE_USER,
};

typedef enum {
        NAME_HAS_OWNER_UNDECIDED = -1,
        NAME_HAS_OWNER_FALSE = 0,
        NAME_HAS_OWNER_TRUE,
        _NAME_HAS_OWNER_MAX
} IsNameHasOwner;

enum busactd_match_type {
        BUSACTD_MATCH_TYPE_PERSISTENT,
        BUSACTD_MATCH_TYPE_RUNTIME,
};

struct busactd_match {
        unsigned int m_id;
        enum busactd_match_type type;
        struct busactd_listener *listener;
        char *sender;
        char *path;
        char *interface;
        char *member;
        char *arg;
};

struct busactd_listener {
        struct busactd *busactd;
        char *busname;
        unsigned int l_id;
        unsigned int ref_count;
        IsNameHasOwner name_has_owner;
        GList *match_list;
};

enum {
        BUSACTD_LOAD_PRESET,
        BUSACTD_LOAD_RUNTIME,
        BUSACTD_LOAD_SYSCONFIG,
        BUSACTD_LOAD_MAX,
};

struct busactd {
        enum busactd_type type;
        GMainLoop *loop;
        struct busactd_dbus *bus;
        GList *listener_list;
        GSource *idle_timeout_source;
        char config_dirs[BUSACTD_LOAD_MAX][PATH_MAX];
};

struct busactd_listener *busactd_listener_new(struct busactd *busactd);
void busactd_listener_free(struct busactd_listener *listener);
void busactd_listener_unref(struct busactd_listener *listener);
struct busactd_listener *busactd_listener_get(struct busactd *busactd, const char *busname);
struct busactd_match *busactd_match_new(struct busactd_listener *listener);
void busactd_match_free(struct busactd_match *match);
void busactd_register_listener(struct busactd_listener *listener);
struct busactd_listener *busactd_add_listener(struct busactd_listener *listener);
void busactd_remove_listener(struct busactd_listener *listener);
struct busactd_match *busactd_add_match(struct busactd_match *match);
void busactd_remove_match(struct busactd_match *match);
struct busactd_match *busactd_find_match_by_id(struct busactd *busactd, unsigned int id);
int busactd_match_new_from_string(struct busactd_listener *listener, const char *string, struct busactd_match **match);
