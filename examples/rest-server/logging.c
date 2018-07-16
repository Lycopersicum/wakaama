/*
 * MIT License
 *
 * Copyright (c) 2018 8devices
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "logging.h"

static long unsigned int stdout_chars = 0, stderr_chars = 0;
static logging_level_t current_level;
static bool timestamp_enabled;
static bool human_readable_timestamp;

int logging_init(logging_settings_t *logging_settings)
{
    current_level = logging_settings->level;

    timestamp_enabled = logging_settings->timestamp ? true : false;
    human_readable_timestamp = logging_settings->human_readable_timestamp ? true : false;

    log_message(LOG_LEVEL_TRACE, "Logging timestamp: %s\n", timestamp_enabled ? "ON" : "OFF");

    log_message(LOG_LEVEL_TRACE, "Logging level set to %d\n", logging_settings->level);

    if (logging_settings->level > LOG_LEVEL_TRACE)
    {
        log_message(LOG_LEVEL_WARN, "Unexpected high log level \"%d\".\n", logging_settings->level);
    }

    return 0;
}

int log_message(logging_level_t logging_level, char *format, ...)
{
    struct timeval time_timeval;
    time_t time_time;
    struct tm *time_tm;
    char time_buffer[64];

    va_list arg_ptr;
    va_start(arg_ptr, format);
    FILE *stream = logging_level <= LOG_LEVEL_ERROR ? stderr : stdout;
    long unsigned int *stream_chars = logging_level <= LOG_LEVEL_ERROR ? &stderr_chars : &stdout_chars;

    if (logging_level > current_level)
    {
        va_end(arg_ptr);
        return -1;
    }

    if (timestamp_enabled && *stream_chars == 0)
    {
        gettimeofday(&time_timeval, NULL);

        if (human_readable_timestamp)
        {
            time_time = time_timeval.tv_sec;
            time_tm = localtime(&time_time);

            strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", time_tm);
            fprintf(stream, "%s.%03lu ", time_buffer, time_timeval.tv_usec % 1000);
        }
        else
        {
            fprintf(stream, "%lu.%03lu ", time_timeval.tv_sec, time_timeval.tv_usec % 1000);
        }
    }

    *stream_chars += vfprintf(stream, format, arg_ptr);

    if (format[strlen(format) - 1] == '\n')
    {
        *stream_chars = 0;
    }

    return 0;
}
