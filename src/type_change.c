/*
 *   Software Updater - server side
 *
 *      Copyright © 2014-2016 Intel Corporation.
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
 *         Tom Keel <thomas.keel@intel.com>
 *
 */

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include "swupd.h"

static char file_type(struct file *file)
{
	if (file->is_file) {
		return 'F';
	}
	if (file->is_dir) {
		return 'D';
	}
	if (file->is_link) {
		return 'L';
	}
	if (file->is_manifest) {
		return 'M';
	}

	// The concept of file type must have been extended...
	fprintf(stderr, "Unknown type for %s .. exiting\n", file->filename);
	exit(EXIT_FAILURE);
	return 0;
}

static bool type_has_changed(struct file *file)
{
	char type1, type2;

	if (file == NULL || file->peer == NULL) {
		// FIXME: Should we assert(0) here? Or not test at all?
		return false;
	}

	if (file->peer->is_deleted) {
		return false;
	}

	if (file->is_deleted) {
		return false;
	}

	type1 = file_type(file->peer);
	type2 = file_type(file);

	if (type1 == type2) {
		return false;
	}

	LOG(file, "Type change", "%c to %c", type1, type2);
	fprintf(stderr, "Type change from %c to %c for %s\n", type1, type2,
			file->filename);

	if (((type1 == 'F') && (type2 == 'L')) ||
	    ((type1 == 'F') && (type2 == 'D')) ||
	    ((type1 == 'L') && (type2 == 'F')) ||
	    ((type1 == 'L') && (type2 == 'D'))) {
		/* 1) file to symlink is an OK transition
		 * 2) file to directory is an OK transition
		 * 3) symlink to file is an OK transition
		 * 4) symlink to directory is an OK transition
		 */
		return false;
	}

	return true;
}

void type_change_detection(struct manifest *manifest)
{
	GList *l;
	int n = 0;

	for (l = g_list_first(manifest->files); l; l = g_list_next(l)) {
		if (type_has_changed(l->data)) {
			n++;
		}
	}

	if (n != 0) {
		printf("Detected %d file type changes in component %s.. exiting\n",
			n, manifest->component);
		exit(EXIT_FAILURE);
	}
}
