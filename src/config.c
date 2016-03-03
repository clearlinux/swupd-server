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
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <glib.h>

#include "swupd.h"

static GKeyFile *keyfile = NULL;

char *config_image_base(void)
{
	assert(keyfile != NULL);

	return g_key_file_get_value(keyfile, "Server", "imagebase", NULL);
}

char *config_output_dir(void)
{
	assert(keyfile != NULL);

	return g_key_file_get_value(keyfile, "Server", "outputdir", NULL);
}

char *config_empty_dir(void)
{
	assert(keyfile != NULL);

	return g_key_file_get_value(keyfile, "Server", "emptydir", NULL);
}

int config_initial_version(void)
{
	assert(keyfile != NULL);
	char *c;
	int version;

	c = g_key_file_get_value(keyfile, "Server", "initialversion", NULL);

	if (!c) {
		return 0;
	}
	version = strtoull(c, NULL, 10);
	free(c);
	return version;
}

bool read_configuration_file(char *filename)
{
	GError *error = NULL;

	keyfile = g_key_file_new();

	if (!g_key_file_load_from_file(keyfile, filename, G_KEY_FILE_NONE, &error)) {
		printf("Failed to Load configuration file: %s (%s)\n", filename, error->message);
		g_error_free(error);
		return false;
	}
#if 0
	char *c;

	printf("Configuration settings:\n");

	c = config_image_base();
	printf("    image base path  : %s\n", c);
	free(c);

	c = config_output_dir();
	printf("    output directory : %s\n", c);
	free(c);

	c = config_empty_dir();
	printf("    empty  directory : %s\n", c);
	free(c);

	printf("\n");
#endif
	return true;
}

void release_configuration_data(void)
{
	if (keyfile != NULL) {
		g_key_file_free(keyfile);
		keyfile = NULL;
	}
}
