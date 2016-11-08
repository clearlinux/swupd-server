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
#include <bsdiff.h>
#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "swupd.h"
#include "xattrs.h"

void __create_delta(struct file *file, int from_version, char *from_hash)
{
	char *original = NULL, *newfile = NULL, *outfile = NULL, *dotfile = NULL, *testnewfile = NULL, *conf = NULL;
	char *tmpdir = NULL;
	char *url = NULL;
	char *cmd = NULL;
	bool delete_original = false;
	struct manifest *manifest = NULL;
	GError *gerror = NULL;
	int ret;

	if (!file->is_file || !file->peer->is_file) {
		return; /* only support deltas between two regular files right now */
	}

	if (file->is_deleted) {
		return; /* file got deleted -> by definition we cannot make a delta */
	}

	if (file->is_dir) {
		return; /* cannot do this for directories yet */
	}

	conf = config_image_base();
	string_or_die(&newfile, "%s/%i/full/%s", conf, file->last_change, file->filename);

	string_or_die(&original, "%s/%i/full/%s", conf, from_version, file->peer->filename);

	if (access(original, F_OK) &&
	    content_url) {
		/* File does not exist. Try to get it from the online update repo instead.
		 * This fallback is meant to be used for CI builds which start with no local
		 * state and only HTTP(S) access to the published www directory.
		 * Not being able to retrieve the file is not an error and will merely
		 * prevent computing the delta.
		 */
		string_or_die(&tmpdir, "%s/make-pack-tmpdir-XXXXXX", state_dir);
		tmpdir = g_dir_make_tmp("make-pack-XXXXXX", &gerror);
		if (!tmpdir) {
			LOG(NULL, "Failed to create temporary directory for untarring original file", "%s",
			    gerror->message);
			assert(0);
		}
		/* Determine hash of original file in the corresponding Manifest. */
		manifest = manifest_from_file(from_version, "full");
		if (!manifest) {
			LOG(NULL, "Failed to read full Manifest", "version %d, cannot retrieve original file",
			    from_version);
			goto out;
		}
		const char *last_hash = NULL;
		GList *list = g_list_first(manifest->files);
		while (list) {
			struct file *original_file = list->data;
			if (!strcmp(file->filename, original_file->filename)) {
				last_hash = original_file->hash;
				break;
			}
			list = g_list_next(list);
		}
		if (!last_hash) {
			LOG(NULL, "Original file not found", "%s in full manifest for %d - inconsistent update data?!",
			    file->filename, from_version);
			goto out;
		}

		/* We use a temporary copy because we don't want to
		 * tamper with the original "full" folder which
		 * probably does not even exist. Using a temporary file
		 * file implies re-downloading in the future, but that's
		 * consistent with the intended usage in a CI environment
		 * which always starts from scratch.
		 */
		free(original);
		string_or_die(&original, "%s/%s", tmpdir, last_hash);
		delete_original = true;

		/*
		 * This is a proof-of-concept. A real implementation should use
		 * a combination of libcurl + libarchive calls to unpack the files.
		 * For current Ostro OS, deltas despite xattr differences would
		 * be needed, otherwise this code here is of little use (all
		 * modified files fail the xattr sameness check, because security.ima
		 * changes when file content changes).
		 */
		string_or_die(&url, "%s/%d/files/%s.tar", content_url, from_version, last_hash);
		LOG(file, "Downloading original file", "%s to %s", url, original);

		/* bsdtar can detect compression when reading from stdin, GNU tar can't. */
		string_or_die(&cmd, "curl -s %s | bsdtar -C %s -xf -", url, tmpdir);
		if (system(cmd)) {
			LOG(file, "Downloading/unpacking failed, skipping delta", "%s", url);
			goto out;
		}
	}

	free(conf);

	conf = config_output_dir();

	string_or_die(&outfile, "%s/%i/delta/%i-%i-%s-%s", conf, file->last_change, from_version, file->last_change, from_hash, file->hash);
	string_or_die(&dotfile, "%s/%i/delta/.%i-%i-%s-%s", conf, file->last_change, from_version, file->last_change, from_hash, file->hash);
	string_or_die(&testnewfile, "%s/%i/delta/.%i-%i-%s-%s.testnewfile", conf, file->last_change, from_version, file->last_change, from_hash, file->hash);

	LOG(file, "Making delta", "%s->%s", original, newfile);

	ret = xattrs_compare(original, newfile);
	if (ret != 0) {
		LOG(NULL, "xattrs have changed, don't create diff ", "%s", newfile);
		goto out;
	}
	ret = make_bsdiff_delta(original, newfile, dotfile, 0);
	if (ret < 0) {
		LOG(file, "Delta creation failed", "%s->%s ret is %i", original, newfile, ret);
		goto out;
	}
	if (ret == 1) {
		LOG(file, "...delta larger than newfile: FULLDL", "%s", newfile);
		unlink(dotfile);
		goto out;
	}

	/* does delta properly recreate expected content? */
	ret = apply_bsdiff_delta(original, testnewfile, dotfile);
	if (ret != 0) {
		printf("Delta application failed.\n");
		printf("Attempted %s->%s via diff %s\n", original, testnewfile, dotfile);
		LOG(file, "Delta application failed.", "Attempted %s->%s via diff %s", original, testnewfile, dotfile);

#warning the above is racy..tolerate it temporarily
		// ok fine
		//unlink(testnewfile);
		//unlink(dotfile);
		ret = 0;
		goto out;
	}
	xattrs_copy(original, testnewfile);

	/* does xattrs have been correctly copied?*/
	if (xattrs_compare(original, testnewfile) != 0) {
		printf("Delta application resulted in xattrs mismatch.\n");
		printf("%s->%s via diff %s yielded %s\n", original, newfile, dotfile, testnewfile);
		LOG(file, "Delta xattrs mismatch:", "%s->%s via diff %s yielded %s", original, newfile, dotfile, testnewfile);
		assert(0);
		goto out;
	}

	char *const sanitycheck[] = { "cmp", "-s", newfile, testnewfile, NULL };
	ret = system_argv(sanitycheck);
	if (ret == -1 || ret == 2) {
		printf("Sanity check system command failed %i. \n", ret);
		printf("%s->%s via diff %s yielded %s\n", original, newfile, dotfile, testnewfile);
		assert(0);
		goto out;
	} else if (ret == 1) {
		printf("Delta application resulted in file mismatch %i. \n", ret);
		printf("%s->%s via diff %s yielded %s\n", original, newfile, dotfile, testnewfile);
		LOG(file, "Delta mismatch:", "%s->%s via diff %s yielded %s", original, newfile, dotfile, testnewfile);

#warning this too will have failures due to races
		//unlink(testnewfile);
		//unlink(dotfile);
		ret = 0;
		goto out;
	}

	unlink(testnewfile);

	if (rename(dotfile, outfile) != 0) {
		if (errno == ENOENT) {
			LOG(NULL, "dotfile:", " %s does not exist", dotfile);
		}
		LOG(NULL, "Failed to rename", "");
	}
out:
	if (delete_original) {
		unlink(original);
	}
	if (tmpdir) {
		rmdir(tmpdir);
		g_free(tmpdir);
	}
	if (manifest) {
		free_manifest(manifest);
	}
	g_clear_error(&gerror);
	free(cmd);
	free(testnewfile);
	free(conf);
	free(newfile);
	free(original);
	free(outfile);
	free(dotfile);
}

void prepare_delta_dir(struct manifest *manifest)
{
	char *path;
	char *conf;

	printf("Preparing delta directory \n");

	conf = config_output_dir();

	string_or_die(&path, "%s/%i", conf, manifest->version);
	if (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
		if (errno != EEXIST) {
			LOG(NULL, "Failed to create directory ", "%s", path);
			return;
		}
	}
	free(path);
	string_or_die(&path, "%s/%i/delta/", conf, manifest->version);
	if (mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
		if (errno != EEXIST) {
			LOG(NULL, "Failed to create directory ", "%s", path);
			return;
		}
	}

	free(path);
	free(conf);
}
