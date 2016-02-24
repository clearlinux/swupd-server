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
#include <sys/types.h>
#include <sys/stat.h>

#include <glib.h>

#include "swupd.h"

void chroot_create_full(int newversion)
{
	int ret;
	char *group;
	char * command;
	char *full_dir;

	string_or_die(&full_dir, "%s/%i/full/", image_dir, newversion);

	g_mkdir_with_parents(full_dir, S_IRWXU);


	/* start with base */
	LOG(NULL, "Copying chroot os-core to full", "");
	string_or_die(&command, "rsync -aAX %s/%i/os-core/ %s",
	              image_dir, newversion, full_dir);
	ret = system(command);
	assert(ret==0);
	free(command);

	/* overlay any new files from each group */
	while (1) {
		group = next_group();
		if (!group) {
			break;
		}

		LOG(NULL, "Overlaying bundle chroot onto full", "%s", group);
		string_or_die(&command, "rsync -aAX --ignore-existing %s/%i/%s/ %s",
		             image_dir, newversion, group, full_dir);
		ret = system(command);
		assert(ret==0);
		free(command);
	}

	/* and since some dumb files have things like install timestamps in
	 * them, we pull files from full back to bundles so each instance
	 * of the "same" file is truly identical across bundles.  It may
	 * make sense at some point to add more logic here for conflict
	 * detection and resolution, including work to insure renames are
	 * consistent across groups. */
	while (1) {
		group = next_group();
		if (!group) {
			break;
		}

		LOG(NULL, "Recopy bundle chroot out of full", "%s", group);
		string_or_die(&command, "rsync -aAX --existing %s %s/%i/%s",
		             full_dir, image_dir, newversion, group);
		ret = system(command);
		assert(ret==0);
		free(command);
	}
	free(full_dir);
}
