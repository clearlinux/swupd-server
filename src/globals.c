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
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swupd.h"

int newversion = -1;
int minversion = 0;
unsigned long long int format = 0;

char *state_dir = NULL;
char *packstage_dir = NULL;
char *image_dir = NULL;
char *staging_dir = NULL;
char *content_url = NULL;

bool set_format(char *userinput)
{
	unsigned long long int user_format;

	// format string shall be a positive integer
	errno = 0;
	user_format = strtoull(userinput, NULL, 10);
	if ((errno < 0) || (user_format == 0)) {
		return false;
	}
	format = user_format;

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

bool set_content_url(const char *url)
{
	if (content_url) {
		free(content_url);
	}
	string_or_die(&content_url, "%s", url);

	return true;
}


bool init_globals(void)
{
	if (format == 0) {
		printf("Error: Missing format parameter. Please specify a format with -F.\n");
		return false;
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
	free(content_url);
}
