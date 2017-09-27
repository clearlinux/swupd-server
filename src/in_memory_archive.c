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

#include <errno.h>
#include <stdlib.h>

#include "libarchive_helper.h"

ssize_t in_memory_write(struct archive *archive, void *client_data, const void *buffer, size_t length)
{
	struct in_memory_archive *in_memory = client_data;
	void *newbuff;

	if (in_memory->maxsize && in_memory->used + length >= in_memory->maxsize) {
		archive_set_error(archive, EFBIG, "resulting archive would become larger than %lu",
				  (unsigned long)in_memory->maxsize);
		archive_write_fail(archive);
		/*
		 * Despite the error and archive_write_fail(), libarchive internally calls us
		 * again and when we fail again, overwrites our error with something about
		 * "Failed to clean up compressor". Therefore our caller needs to check for "used == maxsize"
		 * to detect that we caused the failure.
		 */
		in_memory->used = in_memory->maxsize;
		return -1;
	}

	if (in_memory->used + length > in_memory->allocated) {
		/* Start with a small chunk, double in size to avoid too many reallocs. */
		size_t new_size = in_memory->allocated ?
			in_memory->allocated * 2 :
			4096;
		while (new_size < in_memory->used + length) {
			new_size *= 2;
		}
		newbuff = realloc(in_memory->buffer, new_size);
		if (!newbuff) {
			archive_set_error(archive, ENOMEM, "failed to enlarge buffer");
			return -1;
		}
		in_memory->buffer = newbuff;
		in_memory->allocated = new_size;
	}

	memcpy(in_memory->buffer + in_memory->used, buffer, length);
	in_memory->used += length;
	return length;
}
