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
#include <archive.h>
#include <archive_entry.h>
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
#include <sys/xattr.h>
#include <unistd.h>

#include "swupd.h"
#include "libarchive_helper.h"

/* output must be a file, which is a (compressed) tar file, of the file denoted by "file", without any of its
   directory paths etc etc */
static void create_fullfile(struct file *file)
{
	char *origin = NULL;
	char *tarname = NULL;
	struct stat sbuf;
	char *empty, *indir, *outdir;
	struct archive_entry *entry = NULL;
	struct archive *from = NULL, *to = NULL;
	struct in_memory_archive best = { .buffer = NULL };
	struct in_memory_archive current = { .buffer = NULL };
	uint8_t *file_content = NULL;
	size_t file_size;
	int fd = -1;

	if (file->is_deleted) {
		return; /* file got deleted -> by definition we cannot tar it up */
	}

	empty = config_empty_dir();
	indir = config_image_base();
	outdir = config_output_dir();
	entry = archive_entry_new();
	assert(entry);
	from = archive_read_disk_new();
	assert(from);

	string_or_die(&tarname, "%s/%i/files/%s.tar", outdir, file->last_change, file->hash);
	if (access(tarname, R_OK) == 0) {
		/* output file already exists...done */
		goto done;
		return;
	}

	string_or_die(&origin, "%s/%i/full/%s", indir, file->last_change, file->filename);
	if (lstat(origin, &sbuf) < 0) {
		/* no input file: means earlier phase of update creation failed */
		LOG(NULL, "Failed to stat", "%s: %s", origin, strerror(errno));
		assert(0);
	}

	/* step 1: tar it with each compression type  */
	typedef int (*filter_t)(struct archive *);
	static const filter_t compression_filters[] = {
		/*
		 * Start with the compression method that is most likely (*) to produce
		 * the best result. That will allow aborting creation of archives earlier
		 * when they become larger than the currently smallest archive.
		 *
		 * (*) statistics for ostro-image-swupd:
		 *     43682 LZMA
		 *     13398 gzip
		 *       844 bzip2
		 */
		archive_write_add_filter_lzma,
		archive_write_add_filter_gzip,
		archive_write_add_filter_bzip2,
		/*
		 * TODO (?): can archive_write_add_filter_none ever be better than compressing?
		 */
		NULL
	};
	file_size = S_ISREG(sbuf.st_mode) ? sbuf.st_size : 0;

	archive_entry_copy_sourcepath(entry, origin);
	if (archive_read_disk_entry_from_file(from, entry, -1, &sbuf)) {
		LOG(NULL, "Getting directory attributes failed", "%s: %s",
		    origin, archive_error_string(from));
		assert(0);
	}
	archive_entry_copy_pathname(entry, file->hash);
	if (file_size) {
		file_content = malloc(file_size);
		if (!file_content) {
			LOG(NULL, "out of memory", "");
			assert(0);
		}
		fd = open(origin, O_RDONLY);
		if (fd == -1) {
			LOG(NULL, "Failed to open file", "%s: %s",
			    origin, strerror(errno));
			assert(0);
		}
		size_t done = 0;
		while (done < file_size) {
			ssize_t curr;
			curr = read(fd, file_content + done, file_size - done);
			if (curr == -1) {
				LOG(NULL, "Failed to read from file", "%s: %s",
				    origin, strerror(errno));
				assert(0);
			}
			done += curr;
		}
		close(fd);
		fd = -1;
	}

	for (int i = 0; compression_filters[i]; i++) {
		/* Need to re-initialize the archive handle, it cannot be re-used. */
		if (to) {
			archive_write_free(to);
		}
		/*
		 * Use the recommended restricted pax interchange
		 * format. Numeric uid/gid values are stored in the archive
		 * (no uid/gid lookup enabled) because symbolic names can lead
		 * to a hash mismatch during unpacking when /etc/passwd or
		 * /etc/group change during an update (see
		 * https://github.com/clearlinux/swupd-client/issues/101).
		 *
		 * Filenames read from the file system are expected to be
		 * valid according to the current locale. archive_write_header()
		 * will warn about filenames that it cannot properly decode
		 * and proceeds by writing the raw bytes, but we treat this an
		 * error by not distinguishing between ARCHIVE_FATAL
		 * and ARCHIVE_WARN.
		 *
		 * When we fail with "Can't translate" errors, make sure that
		 * LANG and/or LC_ env variables are set.
		 */
		to = archive_write_new();
		assert(to);
		if (archive_write_set_format_pax_restricted(to)) {
			LOG(NULL, "PAX format", "%s", archive_error_string(to));
			assert(0);
		}
		do {
			/* Try compression methods until we find one which is supported. */
			if (!compression_filters[i](to)) {
				break;
			}
		} while(compression_filters[++i]);
		/*
		 * Regardless of the block size below, never pad the
		 * last block, it just makes the archive larger.
		 */
		if (archive_write_set_bytes_in_last_block(to, 1)) {
			LOG(NULL, "Removing padding failed", "");
			assert(0);
		}
		/*
		 * Invoke in_memory_write() as often as possible and check each
		 * time whether we are already larger than the currently best
		 * algorithm.
		 */
		current.maxsize = best.used;
		if (archive_write_set_bytes_per_block(to, 0)) {
			LOG(NULL, "Removing blocking failed", "");
			assert(0);
		}
		/*
		 * We can make an educated guess how large the resulting archive will be.
		 * Avoids realloc() calls when the file is big.
		 */
		if (!current.allocated) {
			current.allocated = file_size + 4096;
			current.buffer = malloc(current.allocated);
		}
		if (!current.buffer) {
			LOG(NULL, "out of memory", "");
			assert(0);
		}
		if (archive_write_open(to, &current, NULL, in_memory_write, NULL)) {
			LOG(NULL, "Failed to create archive", "%s",
			    archive_error_string(to));
			assert(0);
		}
		if (archive_write_header(to, entry) ||
		    file_content && archive_write_data(to, file_content, file_size) != (ssize_t)file_size ||
		    archive_write_close(to)) {
			if (current.maxsize && current.used >= current.maxsize) {
				archive_write_free(to);
				to = NULL;
				continue;
			}
			LOG(NULL, "Failed to store file in archive", "%s: %s",
			    origin, archive_error_string(to));
			assert(0);
		}
		if (!best.used || current.used < best.used) {
			free(best.buffer);
			best = current;
			memset(&current, 0, sizeof(current));
		} else {
			/* Simply re-use the buffer for the next iteration. */
			current.used = 0;
		}
	}
	if (!best.used) {
		LOG(NULL, "creating archive failed with all compression methods", "");
		assert(0);
	}

	/* step 2: write out to disk. Archives are immutable and thus read-only. */
	fd = open(tarname, O_CREAT|O_WRONLY, S_IRUSR|S_IRGRP|S_IROTH);
	if (fd <= 0) {
		LOG(NULL, "Failed to create archive", "%s: %s",
		    tarname, strerror(errno));
		assert(0);
	}
	size_t done = 0;
	while (done < best.used) {
		ssize_t curr;
		curr = write(fd, best.buffer + done, best.used - done);
		if (curr == -1) {
			LOG(NULL, "Failed to write archive", "%s: %s",
			    tarname, strerror(errno));
			assert(0);
		}
		done += curr;
	}
	if (close(fd)) {
		LOG(NULL, "Failed to complete writing archive", "%s: %s",
		    tarname, strerror(errno));
		assert(0);
	}
	fd = -1;
	free(best.buffer);
	free(current.buffer);
	free(file_content);

 done:
	if (fd >= 0) {
		close(fd);
	}
	archive_read_free(from);
	if (to) {
		archive_write_free(to);
	}
	archive_entry_free(entry);
	free(tarname);
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

	for (; list; list = g_list_next(list)) {
		tmp = list->data;

		// find first new file
		if (tmp->last_change == manifest->version) {
			prev = tmp;
			outfiles = g_list_prepend(outfiles, tmp);
			break;
		}
	}
	for (; list; list = g_list_next(list)) {
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
