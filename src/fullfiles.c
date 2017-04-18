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
 *
 */

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "swupd.h"

/* output must be a file, which is a (compressed) tar file, of the file denoted by "file", without any of its
   directory paths etc etc */
static void create_fullfile(struct file *file)
{
	char *origin;
	char *tarname = NULL;
	char *rename_source = NULL;
	char *rename_target = NULL;
	char *rename_tmpdir = NULL;
	int ret;
	struct stat sbuf;
	char *empty, *indir, *outdir;
	char *param1, *param2;
	int stderrfd;

	if (file->is_deleted) {
		return; /* file got deleted -> by definition we cannot tar it up */
	}

	empty = config_empty_dir();
	indir = config_image_base();
	outdir = config_output_dir();

	string_or_die(&tarname, "%s/%i/files/%s.tar", outdir, file->last_change, file->hash);
	if (access(tarname, R_OK) == 0) {
		/* output file already exists...done */
		free(tarname);
		return;
	}
	free(tarname);
	//printf("%s was missing\n", file->hash);

	string_or_die(&origin, "%s/%i/full/%s", indir, file->last_change, file->filename);
	if (lstat(origin, &sbuf) < 0) {
		/* no input file: means earlier phase of update creation failed */
		LOG(NULL, "Failed to stat", "%s: %s", origin, strerror(errno));
		assert(0);
	}

	if (file->is_dir) { /* directories are easy */
		char *tmp1, *tmp2, *dir, *base;

		tmp1 = strdup(origin);
		assert(tmp1);
		base = basename(tmp1);

		tmp2 = strdup(origin);
		assert(tmp2);
		dir = dirname(tmp2);

		string_or_die(&rename_tmpdir, "%s/XXXXXX", outdir);
		if (!mkdtemp(rename_tmpdir)) {
			LOG(NULL, "Failed to create temporary directory for %s move", origin);
			assert(0);
		}

		string_or_die(&param1, "--exclude=%s/?*", base);
		string_or_die(&param2, "./%s", base);
		char *const tarcfcmd[] = { TAR_COMMAND, "-C", dir, TAR_PERM_ATTR_ARGS_STRLIST, "-cf", "-", param1, param2, NULL };
		char *const tarxfcmd[] = { TAR_COMMAND, "-C", rename_tmpdir, TAR_PERM_ATTR_ARGS_STRLIST, "-xf", "-", NULL };

		stderrfd = open("/dev/null", O_WRONLY);
		if (stderrfd == -1) {
			LOG(NULL, "Failed to open /dev/null", "");
			assert(0);
		}
		if (system_argv_pipe(tarcfcmd, -1, stderrfd, tarxfcmd, -1, stderrfd) != 0) {
			assert(0);
		}
		free(param1);
		free(param2);
		close(stderrfd);

		string_or_die(&rename_source, "%s/%s", rename_tmpdir, base);
		string_or_die(&rename_target, "%s/%s", rename_tmpdir, file->hash);
		if (rename(rename_source, rename_target)) {
			LOG(NULL, "rename failed for %s to %s", rename_source, rename_target);
			assert(0);
		}
		free(rename_source);

		/* for a directory file, tar up simply with gzip */
		string_or_die(&param1, "%s/%i/files/%s.tar", outdir, file->last_change, file->hash);
		char *const tarcmd[] = { TAR_COMMAND, "-C", rename_tmpdir, TAR_PERM_ATTR_ARGS_STRLIST, "-zcf", param1, file->hash, NULL };

		if (system_argv(tarcmd) != 0) {
			assert(0);
		}
		free(param1);

		if (rmdir(rename_target)) {
			LOG(NULL, "rmdir failed for %s", rename_target);
		}
		free(rename_target);
		if (rmdir(rename_tmpdir)) {
			LOG(NULL, "rmdir failed for %s", rename_tmpdir);
		}
		free(rename_tmpdir);

		free(tmp1);
		free(tmp2);
	} else { /* files are more complex */
		char *gzfile = NULL, *bzfile = NULL, *xzfile = NULL;
		char *tempfile;
		uint64_t gz_size = LONG_MAX, bz_size = LONG_MAX, xz_size = LONG_MAX;

		/* step 1: hardlink the guy to an empty directory with the hash as the filename */
		string_or_die(&tempfile, "%s/%s", empty, file->hash);
		if (link(origin, tempfile) < 0) {
			LOG(NULL, "hardlink failed", "%s due to %s (%s -> %s)", file->filename, strerror(errno), origin, tempfile);
			char *const argv[] = { "cp", "-a", origin, tempfile, NULL };
			if (system_argv(argv) != 0) {
				assert(0);
			}
		}

		/* step 2a: tar it with each compression type  */
		// lzma
		string_or_die(&param1, "--directory=%s", empty);
		string_or_die(&param2, "%s/%i/files/%s.tar.xz", outdir, file->last_change, file->hash);
		char *const tarlzmacmd[] = { TAR_COMMAND, param1, TAR_PERM_ATTR_ARGS_STRLIST, "-Jcf", param2, file->hash, NULL };

		if (system_argv(tarlzmacmd) != 0) {
			assert(0);
		}
		free(param1);
		free(param2);

		// gzip
		string_or_die(&param1, "--directory=%s", empty);
		string_or_die(&param2, "%s/%i/files/%s.tar.gz", outdir, file->last_change, file->hash);
		char *const targzipcmd[] = { TAR_COMMAND, param1, TAR_PERM_ATTR_ARGS_STRLIST, "-zcf", param2, file->hash, NULL };

		if (system_argv(targzipcmd) != 0) {
			assert(0);
		}
		free(param1);
		free(param2);

#ifdef SWUPD_WITH_BZIP2
		string_or_die(&param1, "--directory=%s", empty);
		string_or_die(&param2, "%s/%i/files/%s.tar.bz2", outdir, file->last_change, file->hash);
		char *const tarbzip2cmd[] = { TAR_COMMAND, param1, TAR_PERM_ATTR_ARGS_STRLIST, "-jcf", param2, file->hash, NULL };

		if (system_argv(tarbzip2cmd) != 0) {
			assert(0);
		}
		free(param1);
		free(param2);

#endif

		/* step 2b: pick the smallest of the three compression formats */
		string_or_die(&gzfile, "%s/%i/files/%s.tar.gz", outdir, file->last_change, file->hash);
		if (stat(gzfile, &sbuf) == 0) {
			gz_size = sbuf.st_size;
		}
		string_or_die(&bzfile, "%s/%i/files/%s.tar.bz2", outdir, file->last_change, file->hash);
		if (stat(bzfile, &sbuf) == 0) {
			bz_size = sbuf.st_size;
		}
		string_or_die(&xzfile, "%s/%i/files/%s.tar.xz", outdir, file->last_change, file->hash);
		if (stat(xzfile, &sbuf) == 0) {
			xz_size = sbuf.st_size;
		}
		string_or_die(&tarname, "%s/%i/files/%s.tar", outdir, file->last_change, file->hash);
		if (gz_size <= xz_size && gz_size <= bz_size) {
			ret = rename(gzfile, tarname);
		} else if (xz_size <= bz_size) {
			ret = rename(xzfile, tarname);
		} else {
			ret = rename(bzfile, tarname);
		}
		if (ret != 0) {
			LOG(file, "post-tar rename failed", "ret=%d", ret);
		}
		unlink(bzfile);
		unlink(xzfile);
		unlink(gzfile);
		free(bzfile);
		free(xzfile);
		free(gzfile);
		free(tarname);

		/* step 3: remove the hardlink */
		unlink(tempfile);
		free(tempfile);
	}

	free(indir);
	free(outdir);
	free(empty);
	free(origin);
}

static void create_fullfile_task(gpointer data, __unused__ gpointer user_data)
{
	struct file *file = data;

	create_fullfile(file);
}

/* remove duplicate hashes from the fullfile creation list */
static GList *get_deduplicated_fullfile_list(struct manifest *manifest)
{
	GList *list;
	GList *outfiles = NULL;
	struct file *file;
	struct file *prev = NULL;
	struct file *tmp;

	// presort by hash for easy deduplication
	list = manifest->files = g_list_sort(manifest->files, file_sort_hash);

	for (; list ; list = g_list_next(list)) {
		tmp = list->data;

		// find first new file
		if (tmp->last_change == manifest->version) {
			prev = tmp;
			outfiles = g_list_prepend(outfiles, tmp);
			break;
		}
	}
	for (; list ; list = g_list_next(list)) {
		file = list->data;

		// add any new file having a unique hash
		//FIXME: rename logic will be needed here
		if (file->is_deleted || (file->last_change != manifest->version)) {
			continue;
		}
		if (!hash_compare(prev->hash, file->hash)) {
			outfiles = g_list_prepend(outfiles, file);
			prev = file;
		}
	}

	// don't want sort by hash else hash[0] locks will be very conflicted
	outfiles = g_list_sort(outfiles, file_sort_filename);

	return outfiles;
}

static void submit_fullfile_tasks(GList *files)
{
	GThreadPool *threadpool;
	GList *item;
	struct file *file;
	int ret;
	int count = 0;
	GError *err = NULL;
	int numthreads = num_threads(3.0);

	LOG(NULL, "fullfile threadpool", "%d threads", numthreads);
	threadpool = g_thread_pool_new(create_fullfile_task, NULL,
				       numthreads,
				       TRUE, NULL);

	printf("Starting downloadable fullfiles data creation\n");

	item = g_list_first(files);
	while (item) {
		file = item->data;
		item = g_list_next(item);

		ret = g_thread_pool_push(threadpool, file, &err);
		if (ret == FALSE) {
			printf("GThread create_fullfile_task push error\n");
			printf("%s\n", err->message);
			assert(0);
		}
		count++;
	}
	printf("queued %i full file creations\n", count);

	printf("Waiting for downloadable fullfiles data creation to finish\n");
	g_thread_pool_free(threadpool, FALSE, TRUE);
}

void create_fullfiles(struct manifest *manifest)
{
	GList *deduped_file_list;
	char *path;
	char *conf, *empty;

	empty = config_empty_dir();

	char *const rmcmd[] = { "rm", "-rf", empty, NULL };
	if (system_argv(rmcmd) != 0) {
		assert(0);
	}
	g_mkdir_with_parents(empty, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	free(empty);

	conf = config_output_dir();

	string_or_die(&path, "%s/%i", conf, manifest->version);
	if (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
		if (errno != EEXIST) {
			LOG(NULL, "Failed to create directory ", "%s", path);
			return;
		}
	}
	free(path);
	string_or_die(&path, "%s/%i/files/", conf, manifest->version);
	if (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
		if (errno != EEXIST) {
			LOG(NULL, "Failed to create directory ", "%s", path);
			return;
		}
	}
	free(path);
	free(conf);

	/* De-duplicate the list of fullfiles needing created to avoid races */
	deduped_file_list = get_deduplicated_fullfile_list(manifest);

	/* Submit tasks to create full files */
	submit_fullfile_tasks(deduped_file_list);

	g_list_free(deduped_file_list);
}
