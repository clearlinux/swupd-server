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

void __create_delta(struct file *file, int from_version)
{
	char *original, *newfile, *outfile, *dotfile, *testnewfile;
	char *conf, *param1, *param2;
	int ret;

	if (file->is_link) {
		return;
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

	free(conf);

	conf = config_output_dir();

	string_or_die(&outfile, "%s/%i/delta/%i-%i-%s", conf, file->last_change, from_version, file->last_change, file->hash);
	string_or_die(&dotfile, "%s/%i/delta/.%i-%i-%s", conf, file->last_change, from_version, file->last_change, file->hash);
	string_or_die(&testnewfile, "%s/%i/delta/.%i-%i-%s.testnewfile", conf, file->last_change, from_version, file->last_change, file->hash);

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
	xattrs_copy(original, newfile);

	/* does xattrs have been correctly copied?*/
	if (xattrs_compare(original, testnewfile) != 0) {
		printf("Delta application resulted in xattrs mismatch.\n");
		printf("%s->%s via diff %s yielded %s\n", original, newfile, dotfile, testnewfile);
		LOG(file, "Delta xattrs mismatch:", "%s->%s via diff %s yielded %s", original, newfile, dotfile, testnewfile);
		assert(0);
		goto out;
	}

	string_or_die(&param1, "%s", newfile);
	string_or_die(&param2, "%s", testnewfile);
	char *const sanitycheck[] = { "cmp", "-s", param1, param2, NULL};
	ret = system_argv(sanitycheck);
	free(param1);
	free(param2);
	if (ret == -1 || !WIFEXITED(ret) || WEXITSTATUS(ret) == 2) {
		printf("Sanity check system command failed %i. \n", ret);
		printf("%s->%s via diff %s yielded %s\n", original, newfile, dotfile, testnewfile);
		assert(0);
		goto out;
	} else if (WEXITSTATUS(ret) == 1) {
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
