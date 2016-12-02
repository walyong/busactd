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

#define log_full(level, ...)                                            \
        do {                                                            \
                log_write((level), __FILE__, __LINE__, __func__, __VA_ARGS__); \
        } while (0)

#define log_dbg(format, ...)                                    \
        do {                                                    \
                log_full(LOG_DEBUG, format, ##__VA_ARGS__);     \
        } while(0)

#define log_err(format, ...)                                    \
        do {                                                    \
                log_full(LOG_ERR, format, ##__VA_ARGS__);       \
        } while (0)

#define log_info(format, ...)                                   \
        do {                                                    \
                log_full(LOG_INFO, format, ##__VA_ARGS__);      \
        } while (0)

int log_write(int level, const char*file, int line, const char *func, const char *format, ...);
