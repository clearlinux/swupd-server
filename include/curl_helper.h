/*
 *   Software Updater - server side
 *
 *      Copyright © 2017 Intel Corporation.
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

#ifndef __INCLUDE_GUARD_LIBCURL_HELPER_H
#define __INCLUDE_GUARD_LIBCURL_HELPER_H

void curl_helper_free();
int curl_helper_unpack_tar(const char *url, const char *target_dir);

enum {
	CURL_HELPER_OKAY = 0,
	CURL_HELPER_FAILURE
};

#endif /* __INCLUDE_GUARD_LIBCURL_HELPER_H */
