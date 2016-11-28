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
 *         Sebastien Boeuf <sebastien.boeuf@intel.com>
 *         Regis Merlino <regis.merlino@intel.com>
 *         tkeel <thomas.keel@intel.com>
 *
 */

#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "swupd.h"

static GKeyFile *groupfile = NULL;

static char **groups = NULL;
static gsize groupcount = 0;
static unsigned int groupcursor = 0;

#if 0
char *group_packages(char *group)
{
	assert(groupfile != NULL);

	return g_key_file_get_value(groupfile, group, "packages", NULL);
}

char *group_groups(char *group)
{
	assert(groupfile != NULL);

	return g_key_file_get_value(groupfile, group, "groups", NULL);
}
#endif

char *group_status(char *group)
{
	assert(groupfile != NULL);

	return g_key_file_get_value(groupfile, group, "status", NULL);
}

void read_group_file(char *filename)
{
	GError *error = NULL;
	groupfile = g_key_file_new();

	if (g_key_file_load_from_file(groupfile, filename, G_KEY_FILE_NONE, &error)) {
		groups = g_key_file_get_groups(groupfile, &groupcount);
		printf("Found %li groups\n", groupcount);
	} else {
		printf("Failed to Load group file: %s (%s)\n", filename, error->message);
		g_error_free(error);
	}
}

void release_group_file(void)
{
	g_strfreev(groups);
	groups = NULL;
	if (groupfile != NULL) {
		g_key_file_free(groupfile);
		groupfile = NULL;
	}
}

char *next_group(void)
{
	if (groupcursor >= groupcount) {
		groupcursor = 0;
		return NULL;
	}

	return groups[groupcursor++];
}
