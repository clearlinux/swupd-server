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
 *         Christophe Guiraud <christophe.guiraud@intel.com>
 *
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include "swupd.h"
#include "xattrs.h"

enum xattrs_action_type_t_ {
	XATTRS_ACTION_COPY,
	XATTRS_ACTION_GET_BLOB
};

typedef enum xattrs_action_type_t_ xattrs_action_type_t;

static int xattr_get_value(const char *path, const char *name, char **blob,
			   size_t *blob_len, xattrs_action_type_t action)
{
	char *value;
	ssize_t len;

	len = lgetxattr(path, name, NULL, 0);
	if (len < 0) {
		LOG(NULL, "Failed to get x-attribute length",
		    "%s for file %s: %s",
		    name, path, strerror(errno));
		return -1;
	}

	if (*blob == NULL) {
		*blob_len = 0;
	}

	/* realloc needed len + 1 in case we need to add final zero
	 * to ensure consistent blob */
	value = realloc(*blob, *blob_len + len + (action == XATTRS_ACTION_GET_BLOB ? 1 : 0));
	assert(value);

	*blob = value;

	value = value + *blob_len;
	len = lgetxattr(path, name, value, len);
	if (len < 0) {
		LOG(NULL, "Failed to get x-attribute",
		    "%s for file %s: %s",
		    name, path, strerror(errno));
		return -1;
	}

	/* in the xattrs system, value is just an arbitrary binary blob. If it
	 * is a string, it can already be null terminated or not, depending
	 * on the len that was passed when the attribute was set. So, make
	 * sure we use a consistent value by adding a final zero if not
	 * already there.
	 * note: this must not be done when copying attributes. In this case
	 *       we want to keep them unchanged. It must only be done when
	 *       getting the value blob to use for key computation
	 */
	if (action == XATTRS_ACTION_GET_BLOB && len && value[len - 1] != 0) {
		value[len] = 0;
		len++;
	}

	*blob_len += len;

	return 0;
}

static int get_xattr_name_count(const char *names_list, ssize_t len)
{
	int count = 0;
	const char *name;

	for (name = names_list; name < (names_list + len); name += strlen(name) + 1)
		count++;

	return count;
}

static int cmp_xattr_name_ptrs(const void *ptr1, const void *ptr2)
{
	return strcmp(*(char *const *)ptr1, *(char *const *)ptr2);
}

static const char **get_sorted_xattr_name_table(const char *names, int n)
{
	const char **table;
	int i;

	table = calloc(1, n * sizeof(char *));
	assert(table);

	for (i = 0; i < n; i++) {
		table[i] = names;
		names += strlen(names) + 1;
	}

	qsort(table, n, sizeof(char *), cmp_xattr_name_ptrs);

	return table;
}

static void xattrs_do_action(xattrs_action_type_t action,
			     const char *src_filename,
			     const char *dst_filename,
			     char **blob, size_t *blob_len)
{
	ssize_t len;
	char *list;
	int ret = 0;
	char *value = NULL;
	size_t value_len = 0;
	const char **sorted_list = NULL;
	int count;
	int i;
	int offset = 0;

	len = llistxattr(src_filename, NULL, 0);
	if (len <= 0) {
		if (action == XATTRS_ACTION_GET_BLOB) {
			*blob_len = 0;
			*blob = (void *)0xdeadcafe;
		}
		return; // no xattrs, this is OK
	}

	list = calloc(1, len);
	assert(list);

	len = llistxattr(src_filename, list, len);
	if (len <= 0) {
		if (action == XATTRS_ACTION_GET_BLOB) {
			*blob_len = 0;
			*blob = (void *)0xdeadcafe;
		}
		free(list);
		return; // no xattrs, this is OK
	}

	count = get_xattr_name_count(list, len);
	sorted_list = get_sorted_xattr_name_table(list, count);

	if (action == XATTRS_ACTION_GET_BLOB) {
		value = calloc(1, len);
		assert(value);

		value_len = len;

		for (i = 0; i < count; i++) {
			len = strlen(sorted_list[i]) + 1;
			memcpy(value + offset, sorted_list[i], len);
			offset += len;
		}
	}

	for (i = 0; i < count; i++) {
		/* In the XATTRS_ACTION_COPY case the xattr_get_value(...) calls
		 * are always performed with 'value = NULL' and 'value_len = 0'.
		 */
		ret = xattr_get_value(src_filename, sorted_list[i], &value, &value_len,
				      action);
		if (ret < 0) {
			free(value);
			value_len = 0;
			value = NULL;
			break;
		}

		if (action == XATTRS_ACTION_COPY) {
			/* 'value' contains only the attribute value in this
			 * case. */
			ret = lsetxattr(dst_filename, sorted_list[i],
					value, value_len, 0);
			free(value);
			if (ret < 0) {
				LOG(NULL, "Failed to set x-attributes",
				    "%s: %s",
				    dst_filename, strerror(errno));
				break;
			}
			value_len = 0;
			value = NULL;
		}
	}

	if (action == XATTRS_ACTION_GET_BLOB) {
		if (value_len != 0) {
			*blob_len = value_len;
			*blob = value;
		} else {
			*blob_len = 0;
			*blob = (void *)0xdeadcafe;
		}
	}

	free(list);
	free(sorted_list);

	return;
}

void xattrs_copy(const char *src_filename, const char *dst_filename)
{
	xattrs_do_action(XATTRS_ACTION_COPY, src_filename, dst_filename,
			 NULL, NULL);
}

void xattrs_get_blob(const char *filename, char **blob, size_t *blob_len)
{
	xattrs_do_action(XATTRS_ACTION_GET_BLOB, filename, NULL,
			 blob, blob_len);
}

int xattrs_compare(const char *filename1, const char *filename2)
{
	char *new_xattrs;
	char *old_xattrs;
	size_t new_xattrs_len = 0;
	size_t old_xattrs_len = 0;
	int ret = 0;

	xattrs_get_blob(filename1, &old_xattrs, &old_xattrs_len);
	xattrs_get_blob(filename2, &new_xattrs, &new_xattrs_len);

	if ((old_xattrs_len == 0) && (new_xattrs_len == 0)) {
		return ret;
	}

	if ((old_xattrs_len != new_xattrs_len) ||
	    (memcmp(old_xattrs, new_xattrs, old_xattrs_len) != 0)) {
		ret = -1;
	}

	if (old_xattrs_len) {
		free(old_xattrs);
	}
	if (new_xattrs_len) {
		free(new_xattrs);
	}

	return ret;
}
