/*
 *   Software Updater - server side
 *
 *      Copyright © 2012-2016 Intel Corporation.
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, version 2 or later of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Authors:
 *         Arjan van de Ven <arjan@linux.intel.com>
 *         Tim Pepper <timothy.c.pepper@linux.intel.com>
 *
 */

#define _GNU_SOURCE
#include <assert.h>
#include <glib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "swupd.h"

static FILE *logfile[2];

static struct timeval start_time;

void init_log(const char *prefix, const char *bundle, int start, int end)
{
	char *filename;
	string_or_die(&filename, "%s%s-from-%i-to-%i.log", prefix, bundle, start, end);
	logfile[0] = fopen(filename, "w");
	free(filename);
	gettimeofday(&start_time, NULL);
}
void init_log_stdout(void)
{
	logfile[1] = stdout;
	gettimeofday(&start_time, NULL);
}

/* turn a time delta into a decimal string showing elapsed seconds, or an
 * empty string if the time was less than 1ms */
char *get_elapsed_time(struct timeval *t1, struct timeval *t2)
{
	char *elapsed = NULL;
	double d = 0.0;

	// subtract off of "now" the swupd starting time
	t2->tv_sec -= start_time.tv_sec;
	while (t2->tv_usec < start_time.tv_usec) {
		t2->tv_sec--;
		t2->tv_usec += 1000000;
	}
	t2->tv_usec -= start_time.tv_usec;

	// if the start was not "0", subtract to get delta
	if (t1->tv_usec || t1->tv_sec) {
		d = t2->tv_sec * 1000000.0 + t2->tv_usec;
		d -= t1->tv_sec * 1000000.0 + t1->tv_usec;
		d = d / 1000000.0;

		// highlight costly operations by only printing > 1ms deltas
		if (d > 0.001) {
			string_or_die(&elapsed, "%5.3f", d);
		} else {
			string_or_die(&elapsed, " ");
		}
	}
	return elapsed;
}

void __log_message(struct file *file, char *msg, char *filename, int linenr, const char *fmt, ...)
{
	char *buf = NULL;
	struct timeval current_time;
	static struct timeval previous_time;
	va_list ap;
	char *logstring = NULL;
	char filebuf[4096];
	char filebuf2[4096];
	int i;

	if (!logfile[0] && !logfile[1]) {
		return;
	}

	gettimeofday(&current_time, NULL);
	logstring = get_elapsed_time(&previous_time, &current_time);
	previous_time = current_time;

	va_start(ap, fmt);
	if (vasprintf(&buf, fmt, ap) < 0) {
		assert(0);
	}
	va_end(ap);

	filebuf[4095] = 0;
	filebuf2[0] = 0;
	strncpy(filebuf, filename, 4095);
	if (file) {
		strncpy(filebuf2, file->filename, 4095);
	}
	while (strlen(filebuf) < 29) {
		strcat(filebuf, " ");
	}
	while (strlen(filebuf2) < 30) {
		strcat(filebuf2, " ");
	}

	for (i = 0; i < 2; i++) {
		if (logfile[i]) {
			fprintf(logfile[i], "%3i.%03i %5s %s:%03i\t| %s\t| %s\t| %s\n",
				(int)current_time.tv_sec, (int)current_time.tv_usec / 1000, logstring, filebuf, linenr, filebuf2, msg, buf);
			fflush(logfile[i]);
		}
	}

	free(logstring);
	free(buf);
}

void close_log(int version, int exit_status)
{
	struct timeval current_time;
	int t_sec;
	int t_msec;

	if (!logfile[0] && !logfile[1]) {
		return;
	}

	gettimeofday(&current_time, NULL);

	current_time.tv_sec -= start_time.tv_sec;
	while (current_time.tv_usec < start_time.tv_usec) {
		current_time.tv_sec--;
		current_time.tv_usec += 1000000;
	}
	current_time.tv_usec -= start_time.tv_usec;
	t_sec = (int)current_time.tv_sec;
	t_msec = (int)current_time.tv_usec / 1000;

	LOG(NULL, "Update build duration", "%i.%03i seconds", t_sec, t_msec);
	printf("\n\nUpdate build duration was %i.%03i seconds\n", t_sec, t_msec);

	if (exit_status == EXIT_SUCCESS) {
		LOG(NULL, "Update build success", "version %i", version);
		printf("Update complete. System update built for version %i\n", version);
	} else {
		LOG(NULL, "Update build failure", "version %i", version);
		printf("Update build failed for version %i\n", version);
	}

	if (logfile[0]) {
		fclose(logfile[0]);
		logfile[0] = NULL;
	}
}
