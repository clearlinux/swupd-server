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
#include <ctype.h>
#include <glib.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "swupd.h"

#include <magic.h>

static magic_t mcookie;

double rename_score(struct file *old, struct file *new)
{
	double score = 0.0;
	char *c;
	int io, in;

	/* if we have the same hash... sounds like a winner */
	if (hash_compare(old->hash, new->hash)) {
		score += 400;
	}

	/* points for being in the same directory */
	if (strcmp(old->dirname, new->dirname) == 0) {
		score += 10;
	}

	/* points for the basename sharing a beginning */
	c = strchr(old->basename, '.');
	if (c) {
		int len;
		len = c - old->basename;
		if (strncmp(old->basename, new->basename, len) == 0) {
			score += len;
		}
	}

	/* extra points for having an identical basename */
	if (strcmp(old->basename, new->basename) == 0) {
		score += 35;
	}

	/* points for being the same if you remove non-letters */
	if (strcmp(old->alpha_only_filename, new->alpha_only_filename) == 0) {
		score += 50;
	}

	/* walking the directory backwards, for each directory that's the same, five points */
	io = strlen(old->dirname) - 1;
	in = strlen(new->dirname) - 1;
	while (io >= 0 && in >= 0 && old->dirname[io] == new->dirname[in]) {
		if (old->dirname[io] == '/') {
			score += 5;
		}
		io--;
		in--;
	}

	/* if both start with /boot/vmlinuz give it a boost; this is a local hack due to vmlinuz being very short */
	if (strncmp(old->filename, "/boot/vmlinuz", 13) == 0 && strncmp(new->filename, "/boot/vmlinuz", 13) == 0) {
		score += 80;
	}

	/* if ELF, points for sharing the same soname to the first dot */

	/* negative points for not being within 25%+/-1Kb of the same file size */
	if (old->stat.st_size > ((new->stat.st_size * 1.25) + 1024)) {
		score -= 30;
	}
	if (old->stat.st_size < ((new->stat.st_size * 0.75) - 1024)) {
		score -= 30;
	}
	/* zero sized stuff.. never a good idea */
	if (old->stat.st_size == 0 || new->stat.st_size == 0) {
		score = -100;
		return score;
	} else {
		/* size ratio makes a good tiebreaker for sorting; the more the same the more likely to be a match */
		double s;
		s = old->stat.st_size / new->stat.st_size;
		if (s > 1) {
			s = new->stat.st_size / old->stat.st_size;
		}
		score += s;
	}
	/* negative points for not having the same 'file' type */

	if (old->filetype && new->filetype && strcmp(old->filetype, new->filetype) != 0) {
		score -= 60;
	}

	return score;
}

static void precompute_file_data(struct manifest *manifest, struct file *file)
{
	char *c1, *c2;
	char *filename = NULL;
	struct stat buf;

	/* fill in the filename-minus-the-numbers field */
	file->alpha_only_filename = calloc(strlen(file->filename) + 1, sizeof(char));

	c1 = file->filename;
	c2 = file->alpha_only_filename;
	if (c2) {
		for(char c=*c1; c ; c1++) {
			if (isalpha(c)) { /* Only copy letters */
				*c2++ = c;
			}
		}
		/* alpha_only_filename is NUL terminated by calloc */
	}

	if (manifest) {
		string_or_die(&filename, "%s/%i/%s/%s", image_dir, manifest->version, manifest->component, file->filename);
	} else {
		string_or_die(&filename, "%s/%i/full/%s", image_dir, file->last_change, file->filename);
	}

	/* make sure file->stat.st_size is valid */
	if (file->stat.st_size == 0) {
		int ret;

		if (filename == NULL) {
			printf("filename is null...impossible to stat\n");
			assert(0);
		}
		ret = lstat(filename, &buf);
		if (!ret) {
			file->stat.st_size = buf.st_size;
		} else {
			printf("Stat failure on %s\n", filename);
		}
	}

	c1 = (char *)magic_file(mcookie, filename);
	if (c1) {
		char *c2;
		file->filetype = strdup(c1);
		c2 = strstr(file->filetype, "not stripped");
		if (c2) {
			*c2 = 0;
		}
		c2 = strstr(file->filetype, "stripped");
		if (c2) {
			*c2 = 0;
		}
	} else {
		LOG(file, "Cannot find file type", "%s", filename);
	}

	free(filename);

	file->basename = g_path_get_basename(file->filename);
	file->dirname = g_path_get_dirname(file->filename);
}

int file_sort_score(gconstpointer a, gconstpointer b)
{
	struct file *A, *B;

	A = (struct file *)a;
	B = (struct file *)b;

	if (A->rename_score > B->rename_score) {
		return -1;
	}
	if (A->rename_score < B->rename_score) {
		return 1;
	}

	return 0;
}

static void score_file(GList *deleted_files, struct file *file)
{
	GList *list2;
	struct file *file2;

	file->rename_score = -100;

	list2 = g_list_first(deleted_files);
	while (list2) {
		double score;
		file2 = list2->data;
		list2 = g_list_next(list2);

		assert(file2);
		assert(file2->peer);

		score = rename_score(file2->peer, file);
		if (score > file->rename_score) {
			file->rename_score = score;
			file->rename_peer = file2;
		}
	}
}

void rename_detection(struct manifest *manifest)
{
	GList *new_files = NULL;
	GList *deleted_files = NULL;

	GList *list;
	struct file *file;

	if (mcookie == NULL) {
		mcookie = magic_open(MAGIC_NO_CHECK_COMPRESS);
		magic_load(mcookie, NULL);
	}

	/* make a list of new files, no peer */
	list = g_list_first(manifest->files);
	while (list) {
		file = list->data;
		list = g_list_next(list);
		if ((file->last_change != manifest->version) ||
		    (file->is_deleted) ||
		    (!file->is_file) ||
		    (file->peer)) {
			continue;
		}

		new_files = g_list_prepend(new_files, file);
		precompute_file_data(manifest, file);
	}

	/* if there are no new files, we're not having any renames -- early exit */
	if (!new_files) {
		LOG(NULL, "No new files, no rename detection", "%s", manifest->component);
		return;
	}
	/* make a list of newly deleted files that have a peer */

	list = g_list_first(manifest->files);
	while (list) {
		file = list->data;
		list = g_list_next(list);
		if ((!file->is_deleted) ||
		    (!file->peer) ||
		    (file->last_change != manifest->version) ||
		    (file->peer->is_dir || file->peer->is_link)) {
			continue;
		}

		deleted_files = g_list_prepend(deleted_files, file);
		precompute_file_data(NULL, file->peer);
	}

	/* nothing got deleted --> no renames --> early exit */
	if (!deleted_files) {
		LOG(NULL, "No deleted files, no rename detection", "%s", manifest->component);
		g_list_free(new_files);
		return;
	}

	/* for each new file, find the deleted file with the highest score, and store the score */
	list = g_list_first(new_files);
	while (list) {
		file = list->data;
		list = g_list_next(list);
		score_file(deleted_files, file);
	}

	/* sort all new files by score */

	new_files = g_list_sort(new_files, file_sort_score);

	/* pick the top score, check if the score is still valid, if not, recompute the score and resort */

	while (new_files) {
		file = new_files->data;

		if (file->rename_score < 15.0 || file->rename_peer == NULL) {
			new_files = g_list_delete_link(new_files, new_files);
			if (file->rename_peer) {
				LOG(NULL, "Rename not done due to insufficient high score", "%s -> %s   score %4.1f", file->rename_peer->filename, file->filename, file->rename_score);
			}
			continue;
		}

		if (file->rename_peer->rename_peer != NULL) {
			/* the candidate peer got already taken by another file! */
			LOG(NULL, "Rename not done due to target already taken", "%s -> %s   score %4.1f", file->rename_peer->filename, file->filename, file->rename_score);
			file->rename_peer = NULL;
			file->rename_score = -100;
			score_file(deleted_files, file);
			new_files = g_list_sort(new_files, file_sort_score);
			continue;
		}

		/* if valid and score is high enough, make the link by setting the flag and storing the hash */

		LOG(NULL, "Rename detected!", "%s -> %s   score %4.1f", file->rename_peer->filename, file->filename, file->rename_score);

		file->rename_peer->rename_peer = file;
		/* must delete the file from the deleted list */
		deleted_files = g_list_remove(deleted_files, file->rename_peer);
		hash_assign(file->hash, file->rename_peer->hash);
		file->is_rename = 1;
		file->rename_peer->is_rename = 1;

		new_files = g_list_delete_link(new_files, new_files);

	} /* lather, rinse, repeat until all files have a target */

	/* free memory */
	g_list_free(new_files);
	g_list_free(deleted_files);
}

void link_renames(GList *newfiles, int to_version)
{
	GList *list1, *list2;
	GList *targets;
	struct file *file1, *file2;

	targets = newfiles = g_list_sort(newfiles, file_sort_version);

	list1 = g_list_first(newfiles);

	/* todo: sort newfiles and targets by hash */

	while (list1) {
		file1 = list1->data;
		list1 = g_list_next(list1);

		if ((file1->peer || !file1->is_rename) ||
		    (file1->is_deleted)) {
			continue;
		}
		/* now, file1 is the new file that got renamed. time to search the rename targets */
		list2 = g_list_first(targets);
		while (list2) {
			file2 = list2->data;
			list2 = g_list_next(list2);

			if ((!file2->peer || !file2->is_rename) ||
			    (!file2->is_deleted) ||
			    (file2->last_change != to_version)) {
				continue;
			}
			if (hash_compare(file2->hash, file1->hash)) {
				file1->rename_peer = file2->peer;
				file1->peer = file2->peer;
				file2->peer->rename_peer = file1;
				list2 = NULL;
			}
		}
	}
}
