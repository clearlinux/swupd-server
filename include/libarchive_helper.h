/*
 *   Software Updater - server side
 *
 *      Copyright © 2016 Intel Corporation.
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
 *         Patrick Ohly <patrick.ohly@intel.com>
 *
 */

#ifndef __INCLUDE_GUARD_LIBARCHIVE_HELPER_H
#define __INCLUDE_GUARD_LIBARCHIVE_HELPER_H

#include <archive.h>
#include <stdint.h>

/*
 * Used by archive_write_open() callbacks to store the resulting archive in memory.
 */
struct in_memory_archive {
	uint8_t *buffer;
	size_t allocated;
	size_t used;
	/* If not 0, aborts writing when the used data would become larger than this. */
	size_t maxsize;
};

ssize_t in_memory_write(struct archive *, void *client_data, const void *buffer, size_t length);

#endif /* __INCLUDE_GUARD_LIBARCHIVE_HELPER_H */
