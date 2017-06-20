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

#include <sys/syslog.h>
#include <systemd/sd-journal.h>

#define log_dbg(format, ...)                                    \
do {                                                            \
        sd_journal_print(LOG_DEBUG, format, ##__VA_ARGS__);     \
} while(0)

#define log_err(format, ...)                                    \
do {                                                            \
        sd_journal_print(LOG_ERR, format, ##__VA_ARGS__);       \
} while(0)

#define log_info(format, ...)                                   \
do {                                                            \
        sd_journal_print(LOG_INFO, format, ##__VA_ARGS__);      \
} while(0)
