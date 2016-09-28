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

#define _GNU_SOURCE
#include <assert.h>
#include <getopt.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swupd.h"

static const struct option prog_opts[] = {
	{ "help", no_argument, 0, 'h' },
	{ "statedir", required_argument, 0, 'S' },
	{ 0, 0, 0, 0 }
};

static void usage(const char *name)
{
	printf("usage:\n");
	printf("   %s <version>\n\n", name);
	printf("Help options:\n");
	printf("   -h, --help              Show help options\n");
	printf("   -S, --statedir          Optional directory to use for state [ default:=%s ]\n", SWUPD_SERVER_STATE_DIR);
	printf("\n");
}

static bool parse_options(int argc, char **argv)
{
	int opt;

	while ((opt = getopt_long(argc, argv, "hS:", prog_opts, NULL)) != -1) {
		switch (opt) {
		case '?':
		case 'h':
			usage(argv[0]);
			return false;
		case 'S':
			if (!optarg || !set_state_dir(optarg)) {
				printf("Invalid --statedir argument '%s'\n\n", optarg);
				return false;
			}
			break;
		}
	}

	if (!init_state_globals()) {
		return false;
	}

	return true;
}

static void banner(void)
{
	printf(PACKAGE_NAME " update creator -- fullfiles -- version " PACKAGE_VERSION "\n");
	printf("   Copyright (C) 2012-2016 Intel Corporation\n");
	printf("\n");
}

int main(int argc, char **argv)
{
	struct manifest *manifest;
	int version;
	char *file_path = NULL;

	/* keep valgrind working well */
	setenv("G_SLICE", "always-malloc", 0);

	if (!setlocale(LC_ALL, "")) {
		fprintf(stderr, "%s: setlocale() failed\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (!parse_options(argc, argv)) {
		free_state_globals();
		return EXIT_FAILURE;
	}

	if (argc - optind != 1) {
		usage(argv[0]);
		free_state_globals();
		exit(EXIT_FAILURE);
	}

	banner();
	check_root();

	string_or_die(&file_path, "%s/server.ini", state_dir);
	read_configuration_file(file_path);
	free(file_path);

	version = strtoull(argv[optind++], NULL, 10);
	if (version < 0) {
		printf("Usage:\n\tswupd_make_fullfiles <version>\n\n");
		exit(EXIT_FAILURE);
	}

	init_log("swupd-make-fullfiles", "", 0, version);

	manifest = manifest_from_file(version, "full");
	create_fullfiles(manifest);

	free_state_globals();

	return EXIT_SUCCESS;
}
