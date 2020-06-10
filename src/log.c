/**
 * Copyright (c) 2020 Paul-Louis Ageneau
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "log.h"

#ifdef __STDC_NO_THREADS__
#error C11 threads are required
#endif

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <threads.h> // for mutexes
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#define BUFFER_SIZE 4096

static const char *log_level_names[] = {"VERBOSE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

static const char *log_level_colors[] = {
    "\x1B[90m",        // grey
    "\x1B[96m",        // cyan
    "\x1B[39m",        // default foreground
    "\x1B[93m",        // yellow
    "\x1B[91m",        // red
    "\x1B[97m\x1B[41m" // white on red
};

static juice_log_level_t log_level = JUICE_LOG_LEVEL_WARN;
static juice_log_cb_t log_cb = NULL;

static mtx_t log_mutex;
static once_flag log_once_flag = ONCE_FLAG_INIT;

static void log_init(void) { mtx_init(&log_mutex, mtx_plain); }

static bool use_color(void) {
#ifdef _WIN32
	return false;
#else
	return isatty(fileno(stdout)) != 0;
#endif
}

void juice_set_log_level(juice_log_level_t level) {
	call_once(&log_once_flag, log_init);
	mtx_lock(&log_mutex);
	log_level = level;
	mtx_unlock(&log_mutex);
}

void juice_set_log_handler(juice_log_cb_t cb) {
	call_once(&log_once_flag, log_init);
	mtx_lock(&log_mutex);
	log_cb = cb;
	mtx_unlock(&log_mutex);
}

void juice_log_write(juice_log_level_t level, const char *file, int line, const char *fmt, ...) {
	call_once(&log_once_flag, log_init);
	mtx_lock(&log_mutex);
	if (level < log_level) {
		mtx_unlock(&log_mutex);
		return;
	}

	const char *filename = strrchr(file, '/');
	if (filename)
		++filename;
	else
		filename = file;

	if (log_cb) {
		char message[BUFFER_SIZE];
		int len = snprintf(message, BUFFER_SIZE, "%s:%d: ", filename, line);
		len = len >= 0 ? len : 0;

		va_list args;
		va_start(args, fmt);
		len = vsnprintf(message + len, BUFFER_SIZE - len, fmt, args);
		va_end(args);

		if (len >= 0)
			log_cb(level, message);
	} else {
		time_t t = time(NULL);
		struct tm *lt = localtime(&t);
		char buffer[16];
		if (strftime(buffer, 16, "%H:%M:%S", lt) == 0)
			buffer[0] = '\0';

		if (use_color())
			fprintf(stdout, "%s", log_level_colors[level]);

		fprintf(stdout, "%s %-7s %s:%d: ", buffer, log_level_names[level], filename, line);

		va_list args;
		va_start(args, fmt);
		vfprintf(stdout, fmt, args);
		va_end(args);

		if (use_color())
			fprintf(stdout, "%s", "\x1B[0m\x1B[0K");

		fprintf(stdout, "\n");
		fflush(stdout);
	}
	mtx_unlock(&log_mutex);
}
