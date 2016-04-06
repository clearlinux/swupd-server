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
 *         Tom Keel <thomas.keel@intel.com>
 *         Tim Pepper <timothy.c.pepper@linux.intel.com>
 *
 */

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "swupd.h"

static void banner(void)
{
	printf(PACKAGE_NAME " update creator version " PACKAGE_VERSION "\n");
	printf("   Copyright (C) 2012-2016 Intel Corporation\n");
	printf("\n");
}

static const struct option prog_opts[] = {
	{ "help", no_argument, 0, 'h' },
	{ "version", no_argument, 0, 'v' },
	{ "osversion", required_argument, 0, 'o' },
	{ "minversion", required_argument, 0, 'm' },
	{ "format", required_argument, 0, 'F' },
	{ "getformat", no_argument, 0, 'g' },
	{ "statedir", required_argument, 0, 'S' },
	{ "signcontent", no_argument, 0, 's' },
	{ 0, 0, 0, 0 }
};

static void print_help(const char *name)
{
	printf("Usage:\n");
	printf("   %s [OPTION...]\n\n", name);
	printf("Help Options:\n");
	printf("   -h, --help              Show help options\n");
	printf("   -v, --version           Show software version\n");
	printf("\n");
	printf("Application Options:\n");
	printf("   -o, --osversion         The OS version for which to create an update\n");
	printf("   -m, --minversion        Optional minimum file version to write into manifests per file\n");
	printf("   -F, --format            Format number for the update\n");
	printf("   -g, --getformat         Print current format string and exit\n");
	printf("   -S, --statedir          Optional directory to use for state [ default:=%s ]\n", SWUPD_SERVER_STATE_DIR);
	printf("   -s, --signcontent       Enables cryptographic signing of update content\n");
	printf("\n");
}

static bool parse_options(int argc, char **argv)
{
	int opt;

	while ((opt = getopt_long(argc, argv, "hvo:m:F:g:S:s", prog_opts, NULL)) != -1) {
		switch (opt) {
		case '?':
		case 'h':
			print_help(argv[0]);
			return false;
		case 'v':
			banner();
			return false;
		case 'o':
			// expect positive integer
			newversion = strtoull(optarg, NULL, 10);
			if (newversion <= 0) {
				printf("-o | --osversion == %d, requires value > 0 \n", newversion);
				return false;
			}
			break;
		case 'm':
			minversion = strtoull(optarg, NULL, 10);
			if (minversion < 0) {
				printf("-m | --minversion == %d, require value >= 0 \n", minversion);
				return false;
			}
			break;
		case 'F':
			if (!optarg || !set_format(optarg)) {
				printf("Invalid --format argument\n\n");
				return false;
			}
			break;
		case 'S':
			if (!optarg || !set_state_dir(optarg)) {
				printf("Invalid --statedir argument ''%s'\n\n", optarg);
				return false;
			}
			break;
		case 'g':
			if (format == 0) {
				printf("No format specified\n");
			} else {
				printf("%llu\n", format);
				free_globals();
			}
			exit(0);
		case 's':
			enable_signing = true;
			break;
		}
	}

	if (!init_globals()) {
		return false;
	}

	return true;
}

static void populate_dirs(int version)
{
	char *newversiondir;

	string_or_die(&newversiondir, "%s/%d", image_dir, version);

	if ((access(newversiondir, F_OK | R_OK) != 0) && (version == 0)) {
		char *latestpath = NULL;

		string_or_die(&latestpath, "%s/latest.version", image_dir);

		printf("** %s does not exist... creating and populating\n", newversiondir);
		if (mkdir(newversiondir, 0755) != 0) {
			printf("Failed to create directory\n");
		}
		strncat(newversiondir, "/os-core", strlen("/os-core"));
		if (mkdir(newversiondir, 0755) != 0) {
			printf("Failed to create os-core subdirectory\n");
		}

		FILE *latestver;
		latestver = fopen_exclusive(latestpath);
		if (latestver == NULL) {
			printf("Failed to create %s\n", latestpath);
			free(latestpath);
			return;
		}
		if (fwrite("0\n", 2, 1, latestver) != 1) {
			LOG(NULL, "Failed to write latestver", "errno: %d", errno);
		}

		free(latestpath);
		fclose(latestver);
	}
	/* groups don't exist in version 0 */
	if (version != 0) {
		char *group;
		while (1) {
			group = next_group();
			if (!group) {
				break;
			}

			string_or_die(&newversiondir, "%s/%d/%s", image_dir, version, group);

			/* Create the bundle directory(s) as needed */
			if (access(newversiondir, F_OK | R_OK) != 0) {
				printf("%s does not exist...creating\n", group);
				if (mkdir(newversiondir, 0755) != 0) {
					printf("Failed to create %s subdirectory\n", group);
				}
			}
		}
	}
	free(newversiondir);
}

static int check_build_env(void)
{
	char *temp_dir = NULL;
	string_or_die(&temp_dir, "%s/%s", state_dir, "/temp");

	if (access(temp_dir, F_OK | R_OK) != 0) {
		LOG(NULL, "%s", "does not exist...creating directory", temp_dir);
		if (mkdir(temp_dir, 0755) != 0) {
			printf("Failed to create build directory, EXITING\n");
			free(temp_dir);
			return -errno;
		}
	}
	free(temp_dir);

	return 0;
}

int main(int argc, char **argv)
{
	struct manifest *new_core = NULL;
	struct manifest *old_core = NULL;

	struct manifest *new_MoM = NULL;
	struct manifest *old_MoM = NULL;

	struct manifest *old_full = NULL;
	struct manifest *new_full = NULL;

	GHashTable *new_manifests = g_hash_table_new(g_str_hash, g_str_equal);
	GHashTable *old_manifests = g_hash_table_new(g_str_hash, g_str_equal);
	GList *manifests_last_versions_list = NULL;
	int newfiles = 0;
	int old_deleted = 0;

	struct timeval current_time;
	struct timeval previous_time;

	int exit_status = EXIT_FAILURE;
	int ret;

	char *file_path = NULL;

	/* keep valgrind working well */
	setenv("G_SLICE", "always-malloc", 0);

	if (!parse_options(argc, argv)) {
		free_globals();
		return EXIT_FAILURE;
	}

	banner();
	check_root();
	ret = check_build_env();
	if (ret != 0) {
		printf("Failed to setup build environment: ERRNO = %i\n", ret);
		goto exit;
	}

	/* Initilize the crypto signature module */
	if (!signature_initialize()) {
		printf("Can't initialize the crypto signature module!\n");
		goto exit;
	}

	string_or_die(&file_path, "%s/server.ini", state_dir);
	if (!read_configuration_file(file_path)) {
		printf("Failed to read %s configuration file!\n", state_dir);
		free(file_path);
		goto exit;
	}
	free(file_path);

	string_or_die(&file_path, "%s/groups.ini", state_dir);
	read_group_file(file_path);
	free(file_path);

	read_current_version("latest.version");
	printf("Last processed version is %i\n", current_version);

	populate_dirs(newversion);

	printf("Next version is %i \n", newversion);
	init_log("swupd-create-update", "", current_version, newversion);

	gettimeofday(&previous_time, NULL);

	printf("Entering phase 1: full chroot and manifest preparation\n");

	/* Step 1: Create "full" chroot (a union of _everything_ in the build)
	 *         Presume an external entity created chroots for all bundles. */
	printf("Syncing bundle chroots with full chroot\n");
	chroot_create_full(newversion);

	print_elapsed_time(&previous_time, &current_time);

	printf("Calculating full manifest (this is expensive/slow)\n");
	old_full = manifest_from_file(current_version, "full");
	new_full = full_manifest_from_directory(newversion);
	apply_heuristics(old_full);
	apply_heuristics(new_full);
	match_manifests(old_full, new_full);

	old_deleted = remove_old_deleted_files(old_full, new_full);
	if (old_deleted > 0) {
		LOG(NULL, "", "Old deleted files (%d) removed from full manifest", old_deleted);
		printf("Old deleted files (%d) removed from full manifest\n", old_deleted);
	}

	apply_heuristics(new_full);
#warning disabled rename detection for some simplicity
	// rename_detection(new_full);

	print_elapsed_time(&previous_time, &current_time);

	printf("Entering phase 2: Doing the os-core bundle\n");
	/* Phase 2 : the os-core set */

	/* Step 2: Make a manifest for the os-core set */
	old_MoM = manifest_from_file(current_version, "MoM");
	new_MoM = alloc_manifest(newversion, "MoM");
	old_core = manifest_from_file(manifest_subversion(old_MoM, "os-core"), "os-core");
	new_core = sub_manifest_from_directory("os-core", newversion);
	add_component_hashes_to_manifest(new_core, new_full);

	new_core->prevversion = old_core->version;
	apply_heuristics(old_core);
	apply_heuristics(new_core);
	new_MoM->prevversion = old_MoM->version;

	manifests_last_versions_list = get_last_versions_list(newversion, SWUPD_NUM_MANIFEST_DELTAS);

	/* Step 3: Compare to the previous core manifest... */

	if (match_manifests(old_core, new_core) == 0) {
		LOG(NULL, "Core component has not changed, no new manifest", "");
		printf("Core component has not changed, no new manifest\n");
		/* Step 3b: ... and if nothing is new, we stay at the old version */
		new_core->version = old_core->version;
	} else {
		apply_heuristics(new_core);
		/* Step 3c: ... else save the manifest */
		type_change_detection(new_core);

#warning disabled rename detection for some simplicity
		/* Detect renamed files specifically for each pack */
		// rename_detection(...);

		sort_manifest_by_version(new_core);
		newfiles = prune_manifest(new_core);
		old_deleted = remove_old_deleted_files(old_core, new_core);
		if (newfiles <= 0) {
			LOG(NULL, "", "Core component has not changed (after pruning), exiting");
			printf("Core component has not changed (after pruning), exiting\n");
			goto exit;
		}
		LOG(NULL, "", "Core component has changes (%d new, %d deleted), writing out new manifest", newfiles, old_deleted);
		printf("Core component has changes (%d new, %d deleted), writing out new manifest\n", newfiles, old_deleted);
		if (write_manifest(new_core) != 0) {
			LOG(NULL, "", "Core component manifest write failed");
			printf("Core component manifest write failed\n");
			goto exit;
		}
		create_manifest_deltas(new_core, manifests_last_versions_list);
	}

	nest_manifest(new_MoM, new_core);

	/* Phase 3: the functional bundles */
	printf("Entering phase 3: The bundles\n");
	while (1) {
		char *group = next_group();

		if (!group) {
			break;
		}

		(void)g_hash_table_insert(new_manifests, group, sub_manifest_from_directory(group, newversion));
		(void)g_hash_table_insert(old_manifests, group, manifest_from_file(manifest_subversion(old_MoM, group), group));
	}
	while (1) {
		GList *manifest_includes = NULL;
		GList *name_includes;
		char *group = next_group();
		struct manifest *manifest;

		if (!group) {
			break;
		}
		manifest = g_hash_table_lookup(new_manifests, group);
		name_includes = manifest->includes;
		while (name_includes) {
			char *name = name_includes->data;
			name_includes = g_list_next(name_includes);
			manifest_includes = g_list_prepend(manifest_includes, g_hash_table_lookup(new_manifests, name));
		}
		manifest->includes = manifest_includes;
		manifest_includes = NULL;
		manifest = g_hash_table_lookup(old_manifests, group);
		name_includes = manifest->includes;
		while (name_includes) {
			char *name = name_includes->data;
			name_includes = g_list_next(name_includes);
			manifest_includes = g_list_prepend(manifest_includes, g_hash_table_lookup(old_manifests, name));
		}
		manifest->includes = manifest_includes;
	}
	while (1) {
		char *group = next_group();
		struct manifest *oldm;
		struct manifest *newm;

		if (!group) {
			break;
		}

		if (strcmp(group, "os-core") == 0) {
			continue;
		}

		printf("Processing bundle %s\n", group);

		/* Step 4: Make a manifest for this functonal group */
		oldm = g_hash_table_lookup(old_manifests, group);
		newm = g_hash_table_lookup(new_manifests, group);
		add_component_hashes_to_manifest(newm, new_full);
		apply_heuristics(oldm);
		apply_heuristics(newm);
		newm->prevversion = oldm->version;

		/* add os-core as an included manifest */
		if (!manifest_includes(oldm, "os-core")) {
			oldm->includes = g_list_prepend(oldm->includes, old_core);
		}
		if (!manifest_includes(newm, "os-core")) {
			newm->includes = g_list_prepend(newm->includes, new_core);
		}

		/* Step 5: Subtract the core files from the manifest */
		subtract_manifests_frontend(oldm, oldm);
		subtract_manifests_frontend(newm, newm);

		/* Step 6: Compare manifest to the previous version... */
		if (match_manifests(oldm, newm) == 0 && !changed_includes(oldm, newm)) {
			LOG(NULL, "", "%s components have not changed, no new manifest", group);
			printf("%s components have not changed, no new manifest\n", group);
			/* Step 6a: if nothing changed, stay at the old version */
			newm->version = oldm->version;
		} else {
			apply_heuristics(newm);
#warning missing rename_detection here
			/* Step 6b: otherwise, write out the manifest */
			sort_manifest_by_version(newm);
			type_change_detection(newm);
			newfiles = prune_manifest(newm);
			old_deleted = remove_old_deleted_files(oldm, newm);
			if (newfiles > 0 || old_deleted > 0 || changed_includes(oldm, newm)) {
				LOG(NULL, "", "%s component has changes (%d new, %d deleted), writing out new manifest", group, newfiles, old_deleted);
				printf("%s component has changes (%d new, %d deleted), writing out new manifest\n", group, newfiles, old_deleted);
				if (write_manifest(newm) != 0) {
					LOG(NULL, "", "%s component manifest write failed", group);
					printf("%s component manifest write failed\n", group);
					goto exit;
				}
				create_manifest_deltas(newm, manifests_last_versions_list);
			} else {
				LOG(NULL, "", "%s component has not changed (after pruning), no new manifest", group);
				printf("%s component has not changed (after pruning), no new manifest\n", group);
				newm->version = oldm->version;
			}
		}

		nest_manifest(new_MoM, newm);
	}

	print_elapsed_time(&previous_time, &current_time);

	printf("Entering phase 4: completion manifests \n");
	/* Phase 4 : manifest completion */
	/* Step 7: Create the meta-manifest */
	sort_manifest_by_version(new_MoM);
	if (write_manifest(new_MoM) != 0) {
		LOG(NULL, "Failed to write new MoM", "");
		printf("Failed to write new MoM\n");
		goto exit;
	}
	create_manifest_deltas(new_MoM, manifests_last_versions_list);

	print_elapsed_time(&previous_time, &current_time);

	printf("Entering phase 5: creating download content\n");
	/* Phase 5: wrapping up */

	//TODO: should be fixup_versions() and preceed all write_manifest() calls
	maximize_to_full(new_MoM, new_full);

	sort_manifest_by_version(new_full);
	prune_manifest(new_full);
	if (write_manifest(new_full) != 0) {
		goto exit;
	}

	/* Step 8: Prepare delta directory */
	prepare_delta_dir(new_full);

	print_elapsed_time(&previous_time, &current_time);

	exit_status = EXIT_SUCCESS;
exit:
	if (exit_status == EXIT_SUCCESS) {
		write_cookiecrumbs_to_download_area(newversion);
		//write_new_version("latest.version", newversion);
	}
	release_configuration_data();
	release_group_file();
	signature_terminate();
	g_list_free(manifests_last_versions_list);

	close_log(newversion, exit_status);
	printf("Update creation %s\n", exit_status == EXIT_SUCCESS ? "complete" : "failed");

	free_globals();
	return exit_status;
}
