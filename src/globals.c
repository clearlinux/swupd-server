/*
 *   Software Updater - server side
 *
 *      Copyright © 2015-2016 Intel Corporation.
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
 *         Timothy C. Pepper <timothy.c.pepper@linux.intel.com>
 *
 */

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "swupd.h"

int newversion = -1;
int minversion = 0;
char *format_string = NULL;
bool enable_signing = false;

char *state_dir = NULL;
char *packstage_dir = NULL;
char *image_dir = NULL;
char *staging_dir = NULL;

bool set_format_string(char *userinput)
{
	int version;

	// expect a positive integer
	errno = 0;
	version = strtoull(userinput, NULL, 10);
	if ((errno < 0) || (version <= 0)) {
		return false;
	}
	if (format_string) {
		free(format_string);
	}
	string_or_die(&format_string, "%d", version);

	return true;
}

bool set_state_dir(char *dir)
{
	if (dir == NULL || dir[0] == '\0') {
		return false;
	}

	/* TODO: more validation of input? */
	if (dir[0] != '/') {
		printf("statedir must be a full path starting with '/', not '%c'\n", dir[0]);
		return false;
	}

	if (state_dir) {
		free(state_dir);
	}
	string_or_die(&state_dir, "%s", dir);

	return true;
}

bool init_globals(void)
{
	if (format_string == NULL) {
		string_or_die(&format_string, "%d", SWUPD_DEFAULT_FORMAT);
	}

	if (!init_state_globals()) {
		return false;
	}

	if (newversion == -1) {
		printf("Missing version parameter: No new version number specified\n");
		return false;
	}

	printf("file minversion == %d\n", minversion);

	return true;
}

void free_globals(void)
{
	free(format_string);
	free(state_dir);
	free(packstage_dir);
	free(image_dir);
	free(staging_dir);
}

bool init_state_globals(void)
{
	if (state_dir == NULL) {
		string_or_die(&state_dir, "%s", SWUPD_SERVER_STATE_DIR);
	}
	string_or_die(&packstage_dir, "%s/%s", state_dir, "packstage");
	string_or_die(&image_dir, "%s/%s", state_dir, "image");
	string_or_die(&staging_dir, "%s/%s", state_dir, "www");

	return true;
}

void free_state_globals(void)
{
	free(state_dir);
	free(packstage_dir);
	free(image_dir);
	free(staging_dir);
}
