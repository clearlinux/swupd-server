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

#define _GNU_SOURCE
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <glib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "swupd.h"

static void empty_pack_stage(int full, int from_version, int to_version, char *module)
{
	char *param;
	char *path;

	// clean any stale data (eg: re-run after a failure)
	string_or_die(&param, "%s/%s/%i_to_%i/", packstage_dir, module, from_version, to_version);
	char *const rmcmd[] = { "rm", "-fr", param, NULL };
	if (system_argv(rmcmd) != 0) {
		fprintf(stderr, "Failed to clean %s/%s/%i_to_%i\n",
			packstage_dir, module, from_version, to_version);
		free(param);
		exit(EXIT_FAILURE);
	}
	free(param);

	if (!full) {
		// (re)create module/version/{delta,staged}
		string_or_die(&path, "%s/%s/%i_to_%i/delta", packstage_dir, module,
			      from_version, to_version);
		g_mkdir_with_parents(path, S_IRWXU | S_IRWXG);
		free(path);
		string_or_die(&path, "%s/%s/%i_to_%i/staged", packstage_dir, module,
			      from_version, to_version);
		g_mkdir_with_parents(path, S_IRWXU | S_IRWXG);
		free(path);
	}
}

/*
 * for very small files (eh.. lets make that all), we untar them for pack purposes
 */
static void explode_pack_stage(int from_version, int to_version, char *module)
{
	DIR *dir;
	struct dirent *entry;
	struct stat buf;
	char *path;

	string_or_die(&path, "%s/%s/%i_to_%i/staged", packstage_dir, module,
		      from_version, to_version);
	g_mkdir_with_parents(path, S_IRWXU | S_IRWXG);
	dir = opendir(path);
	if (!dir) {
		fprintf(stderr, "There are problems accessing %s, exiting\n", path);
		exit(EXIT_FAILURE);
	}
	free(path);
	while (1) {
		char *path, *param;
		int ret;

		entry = readdir(dir);
		if (!entry) {
			break;
		}

		if ((strcmp(entry->d_name, ".") == 0) ||
		    (strcmp(entry->d_name, "..") == 0) ||
		    (strstr(entry->d_name, ".tar") == NULL)) {
			continue;
		}

		string_or_die(&path, "%s/%s/%i_to_%i/staged/%s", packstage_dir, module,
			      from_version, to_version, entry->d_name);
		ret = stat(path, &buf);
		if (ret) {
			free(path);
			continue;
		}

		/*
		 * initially, we only untar'd small files and broke out of the loop here.
		 * However, it turns out that even for larger files it's worth uncompressing,
		 * the resulting pack is slightly smaller, and in addition, we're saving CPU
		 * time on the client...
		 */
		string_or_die(&param, "%s/%s/%i_to_%i/staged", packstage_dir, module, from_version, to_version);
		char *const tarcmd[] = { TAR_COMMAND, "-C", param, TAR_WARN_ARGS_STRLIST TAR_PERM_ATTR_ARGS_STRLIST, "-xf", path, NULL };
		if (system_argv(tarcmd) == 0) {
			unlink(path);
		}
		free(param);
		free(path);
	}
	closedir(dir);
}

static void prepare_pack(struct packdata *pack)
{
	struct manifest *manifest;

	pack->fullcount = 0;

	manifest = manifest_from_file(pack->from, pack->module);
	if (!manifest || ((manifest->count == 0) && (manifest->version > 0))) {
		free(manifest);
		return;
	}

	pack->end_manifest = manifest_from_file(pack->to, pack->module);

	empty_pack_stage(0, pack->from, pack->to, pack->module);

	match_manifests(manifest, pack->end_manifest);

	pack->end_manifest->files = link_renames(pack->end_manifest->files, pack->to);
}

static void make_pack_full_files(struct packdata *pack)
{
	GList *item;
	struct file *file;
	int ret;

	LOG(NULL, "starting pack full file creation", "%s: %d to %d",
	    pack->module, pack->from, pack->to);

	/* 	full files pack: */
	item = g_list_first(pack->end_manifest->files);
	while (item) {
		file = item->data;
		item = g_list_next(item);
		if ((!file->peer || file->peer->is_deleted) && !file->is_deleted && !file->rename_peer) {
			char *from, *to;
			char *fullfrom, *fullto;

			/* hardlink each file that is in <end> but not in <X> */
			string_or_die(&fullfrom, "%s/%i/full/%s", image_dir, file->last_change, file->filename);
			string_or_die(&fullto, "%s/%s/%i_to_%i/staged/%s", packstage_dir,
				      pack->module, pack->from, pack->to, file->hash);
			string_or_die(&from, "%s/%i/files/%s.tar", staging_dir, file->last_change, file->hash);
			string_or_die(&to, "%s/%s/%i_to_%i/staged/%s.tar", packstage_dir,
				      pack->module, pack->from, pack->to, file->hash);

			ret = -1;
			errno = 0;

			/* Prefer to hardlink uncompressed files (excluding
			 * directories) first, and fall back to the compressed
			 * versions if the hardlink fails.
			 */
			if (!file->is_dir) {
				ret = link(fullfrom, fullto);
				if (ret && errno != EEXIST) {
					LOG(NULL, "Failure to link for fullfile pack", "%s to %s (%s) %i", fullfrom, fullto, strerror(errno), errno);
				}
			}
			if (ret) {
				ret = link(from, to);
				if (ret && errno != EEXIST) {
					LOG(NULL, "Failure to link for fullfile pack", "%s to %s (%s) %i", from, to, strerror(errno), errno);
				}
			}

			if (ret == 0) {
				pack->fullcount++;
			}

			free(from);
			free(to);
			free(fullfrom);
			free(fullto);
		}
	}

	LOG(NULL, "finished pack full file creation", "%s: %d to %d",
	    pack->module, pack->from, pack->to);
}

static int find_file_in_list(GList *files, struct file *file)
{
	GList *item;
	struct file *item_file;

	item = g_list_first(files);

	while (item) {
		item_file = item->data;
		item = g_list_next(item);

		if (hash_compare(item_file->hash, file->hash) &&
		    (item_file->last_change == file->last_change) &&
		    (item_file->peer->last_change == file->peer->last_change)) {
			LOG(NULL, "Found a duplicate delta", "%d %d %s %s", file->peer->last_change, file->last_change, file->hash, file->filename);
			return TRUE;
		}
	}

	return FALSE;
}

static GList *consolidate_packs_delta_files(GList *files, struct packdata *pack)
{
	GList *item;
	struct file *file;
	char *from;
	struct stat stat_delta;
	int ret;

	if (!pack->end_manifest) {
		return files;
	}

	item = g_list_first(pack->end_manifest->files);

	while (item) {
		file = item->data;
		item = g_list_next(item);

		if ((file->last_change <= pack->from) ||
		    (!file->peer) ||
		    (!file->is_file && !file->is_dir && !file->is_link)) {
			continue;
		}

		string_or_die(&from, "%s/%i/delta/%i-%i-%s-%s", staging_dir, file->last_change,
			      file->peer->last_change, file->last_change, file->peer->hash, file->hash);

		ret = stat(from, &stat_delta);
		if (ret && !find_file_in_list(files, file)) {
			files = g_list_prepend(files, file);
		}

		free(from);
	}

	return files;
}

static void create_delta(gpointer data, __unused__ gpointer user_data)
{
	struct file *file = data;

	/* if the file was not found in the from version, skip delta creation */
	if (file->peer) {
		__create_delta(file, file->peer->last_change, file->peer->hash);
	}
}

static void make_pack_deltas(GList *files)
{
	GThreadPool *threadpool;
	GList *item;
	struct file *file;
	int ret;
	GError *err = NULL;
	int numthreads = num_threads(1.0);

	LOG(NULL, "pack deltas threadpool", "%d threads", numthreads);
	threadpool = g_thread_pool_new(create_delta, NULL,
				       numthreads, FALSE, NULL);

	item = g_list_first(files);
	while (item) {
		file = item->data;
		item = g_list_next(item);

		ret = g_thread_pool_push(threadpool, file, &err);
		if (ret == FALSE) {
			// intentionally non-fatal
			fprintf(stderr, "GThread create_delta push error\n");
			fprintf(stderr, "%s\n", err->message);
			return;
		}
	}

	g_thread_pool_free(threadpool, FALSE, TRUE);
}

/* Returns 0 == success, other == failure */
static int make_final_pack(struct packdata *pack)
{
	GList *item;
	struct file *file;
	int ret;
	char *param1, *param2;
	double penalty;

	LOG(NULL, "make_final_pack", "%s: %i to %i", pack->module, pack->from, pack->to);

	item = g_list_first(pack->end_manifest->files);

	while (item) {
		char *from, *to, *tarfrom, *tarto, *fullfrom, *fullto;
		struct stat stat_delta, stat_tar;

		file = item->data;
		item = g_list_next(item);

		if ((file->last_change <= pack->from) ||
		    (!file->peer) ||
		    (!file->is_file && !file->is_dir && !file->is_link)) {
			continue;
		}

		/* for each file changed since <X> */
		/* locate delta, check if the diff it's from is >= <X> */
		string_or_die(&from, "%s/%i/delta/%i-%i-%s-%s", staging_dir, file->last_change,
			      file->peer->last_change, file->last_change, file->peer->hash, file->hash);
		string_or_die(&to, "%s/%s/%i_to_%i/delta/%i-%i-%s-%s", packstage_dir,
			      pack->module, pack->from, pack->to, file->peer->last_change,
			      file->last_change, file->peer->hash, file->hash);
		string_or_die(&tarfrom, "%s/%i/files/%s.tar", staging_dir,
			      file->last_change, file->hash);
		string_or_die(&tarto, "%s/%s/%i_to_%i/staged/%s.tar", packstage_dir,
			      pack->module, pack->from, pack->to, file->hash);
		string_or_die(&fullfrom, "%s/%i/full/%s", image_dir, file->last_change, file->filename);
		string_or_die(&fullto, "%s/%s/%i_to_%i/staged/%s", packstage_dir,
			      pack->module, pack->from, pack->to, file->hash);

		ret = stat(from, &stat_delta);
		if (ret) {
			stat_delta.st_size = LONG_MAX;
		}

		ret = stat(tarfrom, &stat_tar);
		if (ret) {
			stat_tar.st_size = LONG_MAX;
		}

		if (file->is_dir) {
			stat_delta.st_size = LONG_MAX;
		}

		if (stat_delta.st_size <= 8) {
			stat_delta.st_size = LONG_MAX;
		}

		penalty = 1.05 * (double)(stat_delta.st_size);

		/* delta files get a 5% penalty, they are more cpu work on the client */
		if ((penalty < (double)(stat_tar.st_size)) || stat_tar.st_size == 0) {
			/* include delta file in pack */
			ret = link(from, to);
			if (ret) {
				if (errno != EEXIST) {
					LOG(NULL, "Failure to link", "%s to %s (%s) %i\n", from, to, strerror(errno), errno);
				}
			}
		} else {
			ret = -1;
			errno = 0;

			/* Prefer to hardlink uncompressed files (excluding
			 * directories) first, and fall back to the compressed
			 * versions if the hardlink fails.
			 */
			if (!file->is_dir) {
				ret = link(fullfrom, fullto);
				if (ret && errno != EEXIST) {
					LOG(NULL, "Failure to link for final pack", "%s to %s (%s) %i\n", fullfrom, fullto, strerror(errno), errno);
				}
			}

			if (ret) {
				ret = link(tarfrom, tarto);
				if (ret && errno != EEXIST) {
					LOG(NULL, "Failure to link for final pack", "%s to %s (%s) %i\n", tarfrom, tarto, strerror(errno), errno);
				}
			}

			if (ret == 0) {
				pack->fullcount++;
			}
		}

		free(from);
		free(to);
		free(tarfrom);
		free(tarto);
		free(fullfrom);
		free(fullto);
	}

	if (pack->fullcount > 0) {
		/* untar all files for smaller size and less cpu use on client */
		explode_pack_stage(pack->from, pack->to, pack->module);
	}

	char *bundle_delta = NULL;

	/* now... link in the Manifest pack */
	if (pack->from != 0) {
		char *from, *to;
		struct stat st;

		string_or_die(&from, "%s/%i/Manifest-%s-delta-from-%i",
			      staging_dir, pack->to, pack->module, pack->from);

		ret = stat(from, &st);
		if (ret) {
			LOG(NULL, "Making extra manifest delta", "%s: %i->%i", pack->module, pack->from, pack->to);
			create_manifest_delta(pack->from, pack->to, pack->module);
		}

		string_or_die(&to, "%s/%s/%i_to_%i/Manifest-%s-delta-from-%i", packstage_dir,
			      pack->module, pack->from, pack->to, pack->module, pack->from);

		ret = link(from, to);
		if (ret) {
			LOG(NULL, "Failed to link", "Manifest-%s-delta-from-%i (%s)", pack->module, pack->from, strerror(errno));
		} else {
			string_or_die(&bundle_delta, "Manifest-%s-delta-from-%i", pack->module, pack->from);
		}

		free(from);
		free(to);
	}

	char *mom_delta = NULL;

	/* now... link in the MoM Manifest into the os-core pack */
	if ((pack->from != 0) && (strcmp(pack->module, "os-core") == 0)) {
		char *from, *to;
		struct stat st;

		string_or_die(&from, "%s/%i/Manifest-%s-delta-from-%i",
			      staging_dir, pack->to, "MoM", pack->from);
		ret = stat(from, &st);
		if (ret) {
			LOG(NULL, "Making extra manifest delta", "MoM: %i->%i", pack->from, pack->to);
			create_manifest_delta(pack->from, pack->to, "MoM");
		}

		string_or_die(&to, "%s/%s/%i_to_%i/Manifest-%s-delta-from-%i", packstage_dir,
			      pack->module, pack->from, pack->to, "MoM", pack->from);

		ret = link(from, to);
		if (ret) {
			LOG(NULL, "Failed to link", "Manifest-MoM-delta-from-%i (%s)", pack->from, strerror(errno));
		} else {
			string_or_die(&mom_delta, "Manifest-MoM-delta-from-%i", pack->from);
		}

		free(from);
		free(to);
	}

	/* tar the staging directory up */
	LOG(NULL, "starting tar for pack", "%s: %i to %i", pack->module, pack->from, pack->to);
	string_or_die(&param1, "%s/%s/%i_to_%i/", packstage_dir, pack->module, pack->from, pack->to);
	string_or_die(&param2, "%s/%i/pack-%s-from-%i.tar", staging_dir, pack->to, pack->module, pack->from);
	char *const tarcmd[] = { TAR_COMMAND, "-C", param1, TAR_PERM_ATTR_ARGS_STRLIST, "--numeric-owner", "-Jcf", param2, "delta", "staged", bundle_delta, mom_delta, NULL };
	ret = system_argv(tarcmd);
	free(param1);
	free(param2);
	LOG(NULL, "finished tar for pack", "%s: %i to %i", pack->module, pack->from, pack->to);
	/* FIXME: this is a hack workaround, needs diagnosed and removed */
	if ((ret != 0) && (ret != 1)) {
		fprintf(stderr, "Unexpected return value (%d) creating tar of pack %s from %i to %i\n",
			ret, pack->module, pack->from, pack->to);
	}

	/* and clean up */
	free_manifest(pack->end_manifest);
	empty_pack_stage(1, pack->from, pack->to, pack->module);

	LOG(NULL, "pack complete", "%s: %i to %i", pack->module, pack->from, pack->to);
	return ret;
}

/* Returns 0 == success, -1 == failure */
int make_pack(struct packdata *pack)
{
	GList *delta_list = NULL;

	/* step 1: prepare pack */
	prepare_pack(pack);

	/* step 2: consolidate delta list & create all delta files*/
	delta_list = consolidate_packs_delta_files(delta_list, pack);
	make_pack_deltas(delta_list);
	g_list_free(delta_list);

	/* step 3: complete pack creation */
	if (!pack->end_manifest) {
		return 0;
	}
	make_pack_full_files(pack);
	make_final_pack(pack);

	return 0;
}
