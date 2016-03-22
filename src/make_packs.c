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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <getopt.h>

#include <glib.h>

#include <getopt.h>

#include "swupd.h"

static void banner(void)
{
	printf(PACKAGE_NAME "update pack creator version " PACKAGE_VERSION "\n");
	printf("   Copyright (C) 2012-2016 Intel Corporation\n");
	printf("\n");
}

static const struct option prog_opts[] = {
	{ "help", no_argument, 0, 'h' },
	{ "statedir", required_argument, 0, 'S' },
	{ "signcontent", no_argument, 0, 's' },
	{ 0, 0, 0, 0 }
};

static void usage(const char *name)
{
	printf("usage:\n");
	printf("   %s <start version> <latest version> <bundle>\n\n", name);
	printf("Help options:\n");
	printf("   -h, --help              Show help options\n");
	printf("   -S, --statedir          Optional directory to use for state [ default:=%s ]\n", SWUPD_SERVER_STATE_DIR);
	printf("   -s, --signcontent       Enables cryptographic signing of update content\n");
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
				printf("Invalid --statedir argument ''%s'\n\n", optarg);
				return false;
			}
			break;
		case 's':
			enable_signing = true;
			break;
		}
	}

	/* FIXME: *_state_globals() are ugly hacks */
	if (!init_state_globals()) {
		return false;
	}

	return true;
}

int main(int argc, char **argv)
{
	int start_version;
	long end_version;
	//long version;
	char *module;
	struct packdata *pack;
	int exit_status = EXIT_FAILURE;
	char *file_path = NULL;

	if (!parse_options(argc, argv)) {
		free_state_globals();
		return EXIT_FAILURE;
	}

	if (argc - optind != 3) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	banner();
	check_root();

	/* Initilize the crypto signature module */
	if (!signature_initialize()) {
		printf("Can't initialize the crypto signature module!\n");
		return exit_status;
	}

	string_or_die(&file_path, "%s/server.ini", state_dir);
	read_configuration_file(file_path);
	free(file_path);

	start_version = strtoull(argv[optind++], NULL, 10);
	end_version = strtoull(argv[optind++], NULL, 10);
	module = argv[optind++];

	if ((start_version < 0) ||
	    (end_version == 0) ||
	    (start_version >= end_version)) {
		printf("Invalid version combination: %i - %li \n", start_version, end_version);
		exit(EXIT_FAILURE);
	}

	init_log("swupd-make-pack-", module, start_version, end_version);

	printf("Making pack-%s %i to %li\n", module, start_version, end_version);

	pack = calloc(1, sizeof(struct packdata));
	if (pack == NULL) {
		assert(0);
	}

	pack->module = module;
	pack->from = start_version;
	pack->to = end_version;

	if (make_pack(pack) == 0) {
		exit_status = EXIT_SUCCESS;
	}

	signature_terminate();

	printf("Pack creation %s (pack-%s %i to %li)\n",
	       exit_status == EXIT_SUCCESS ? "complete" : "failed",
	       module, start_version, end_version);

	free_state_globals();
	return exit_status;
}
