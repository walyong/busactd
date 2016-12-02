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
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>

#include <libsystem/libsystem.h>

#include "log.h"

static int _log_write(
                int level,
                const char *file,
                int line,
                const char *func,
                const char *format,
                va_list ap) {

        char buff[LINE_MAX];

        vsnprintf(buff, LINE_MAX, format, ap);
        fprintf(level <= LOG_ERR ? stderr : stdout, "%s\n", buff);

        return 0;
}

int log_write(
                int level,
                const char*file,
                int line,
                const char *func,
                const char *format, ...) {

        va_list ap;
        int r;

        va_start(ap, format);
        r = _log_write(level, file, line, func, format, ap);
        va_end(ap);

        return r;
}
