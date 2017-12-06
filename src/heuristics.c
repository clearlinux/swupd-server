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
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swupd.h"

/* TODO: this needs to come from a file that has a list of regexps */

static void config_file_heuristics(struct file *file)
{
	if ((strncmp(file->filename, "/etc/", 5) == 0)) {
		file->is_config = 1;
		LOG(NULL, "Config file marked", "%s", file->filename);
	}
}

static void runtime_state_heuristics(struct file *file)
{
	/* these are shipped directories that are not themselves state,
	 * rather only their contents are state */
	if ((strcmp(file->filename, "/usr/src/debug") == 0) ||
	    (strcmp(file->filename, "/usr/src/kernel") == 0) ||
	    (strcmp(file->filename, "/dev") == 0) ||
	    (strcmp(file->filename, "/home") == 0) ||
	    (strcmp(file->filename, "/proc") == 0) ||
	    (strcmp(file->filename, "/root") == 0) ||
	    (strcmp(file->filename, "/run") == 0) ||
	    (strcmp(file->filename, "/sys") == 0) ||
	    (strcmp(file->filename, "/tmp") == 0) ||
	    (strcmp(file->filename, "/var") == 0)) {
		return;
	}

	/* the contents of these directory are not state,
	 * but it belongs to a state directory  */
	if ((strncmp(file->filename, "/usr/src/kernel/", 16) == 0)) {
		return;
	}

	/* the contents of these directory are state, ideally this never
	 * triggers if our package builds are clean */
	if ((strncmp(file->filename, "/dev/", 5) == 0) ||
	    (strncmp(file->filename, "/home/", 6) == 0) ||
	    (strncmp(file->filename, "/proc/", 6) == 0) ||
	    (strncmp(file->filename, "/root/", 6) == 0) ||
	    (strncmp(file->filename, "/run/", 5) == 0) ||
	    (strncmp(file->filename, "/sys/", 5) == 0) ||
	    (strncmp(file->filename, "/tmp/", 5) == 0) ||
	    (strncmp(file->filename, "/var/", 5) == 0) ||
	    (strncmp(file->filename, "/usr/src/", 9) == 0)) {
		file->is_state = 1;
		LOG(NULL, "Bad package runtime state detected", "%s", file->filename);
		return;
	}

	/* these are commonly added directories for user customization,
	 * ideally this never triggers if our package builds are clean */
	if ((strncmp(file->filename, "/acct", 5) == 0) ||
	    (strncmp(file->filename, "/cache", 6) == 0) ||
	    (strncmp(file->filename, "/data", 5) == 0) ||
	    (strncmp(file->filename, "/lost+found", 11) == 0) ||
	    (strncmp(file->filename, "/mnt/asec", 9) == 0) ||
	    (strncmp(file->filename, "/mnt/obb", 8) == 0) ||
	    (strncmp(file->filename, "/mnt/shell/emulated", 19) == 0) ||
	    (strncmp(file->filename, "/mnt/swupd", 10) == 0) ||
	    (strncmp(file->filename, "/oem", 4) == 0) ||
	    (strncmp(file->filename, "/system/rt/audio", 16) == 0) ||
	    (strncmp(file->filename, "/system/rt/gfx", 14) == 0) ||
	    (strncmp(file->filename, "/system/rt/media", 16) == 0) ||
	    (strncmp(file->filename, "/system/rt/wifi", 15) == 0) ||
	    (strncmp(file->filename, "/system/etc/firmware/virtual", 28) == 0)) {
		file->is_state = 1;
		LOG(NULL, "Surprising package runtime state detected", "%s", file->filename);
		return;
	}
}

static void boot_file_heuristics(struct file *file)
{
	if ((strncmp(file->filename, "/boot/", 6) == 0) ||
	    (strncmp(file->filename, "/usr/lib/modules/", 17) == 0) ||
	    (strncmp(file->filename, "/usr/lib/kernel/", 16) == 0) ||
	    (strncmp(file->filename, "/usr/share/kernel/", 18) == 0) ||
	    (strncmp(file->filename, "/usr/lib/gummiboot", 18) == 0) ||
	    (strncmp(file->filename, "/usr/bin/gummiboot", 18) == 0)) {
		file->is_boot = 1;
		// LOG(NULL, "Boot file marked", "%s", file->filename);
	}
}

void apply_heuristics(struct manifest *manifest)
{
	GList *list;
	struct file *file;

	list = g_list_first(manifest->files);
	while (list) {
		file = list->data;
		list = g_list_next(list);

		config_file_heuristics(file);
		runtime_state_heuristics(file);
		boot_file_heuristics(file);
	}
}
