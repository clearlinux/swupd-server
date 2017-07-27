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
 *         cguiraud <christophe.guiraud@intel.com>
 *         Tim Pepper <timothy.c.pepper@linux.intel.com>
 *
 */

#define _GNU_SOURCE
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <glib.h>
#include <linux/limits.h>
#include <openssl/hmac.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "swupd.h"
#include "xattrs.h"

static GThreadPool *threadpool;

/* Why not strcpy? Looks like the hash was going to be stored in
 * binary at one stage. Should use g_string_chunk_insert_const to
 * change hash_compare to a pointer compare
 */
void hash_assign(char *src, char *dst)
{
	memcpy(dst, src, SWUPD_HASH_LEN - 1);
	dst[SWUPD_HASH_LEN - 1] = '\0';
}

bool hash_compare(char *hash1, char *hash2)
{
	if (bcmp(hash1, hash2, SWUPD_HASH_LEN - 1) == 0) {
		return true;
	} else {
		return false;
	}
}

bool hash_is_zeros(char *hash)
{
	return hash_compare("0000000000000000000000000000000000000000000000000000000000000000", hash);
}

void hash_set_zeros(char *hash)
{
	hash_assign("0000000000000000000000000000000000000000000000000000000000000000", hash);
}

static void hmac_sha256_for_data(char *hash,
				 const unsigned char *key, size_t key_len,
				 const unsigned char *data, size_t data_len)
{
	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int digest_len = 0;
	char *digest_str;
	unsigned int i;

	if (data == NULL) {
		hash_set_zeros(hash);
		return;
	}

	if (HMAC(EVP_sha256(), (const void *)key, key_len, data, data_len, digest, &digest_len) == NULL) {
		hash_set_zeros(hash);
		return;
	}

	digest_str = calloc((digest_len * 2) + 1, sizeof(char));
	if (digest_str == NULL) {
		assert(0);
	}

	for (i = 0; i < digest_len; i++) {
		sprintf(&digest_str[i * 2], "%02x", (unsigned int)digest[i]);
	}

	hash_assign(digest_str, hash);
	free(digest_str);
}

static void hmac_sha256_for_string(char *hash,
				   const unsigned char *key, size_t key_len,
				   const char *str)
{
	if (str == NULL) {
		hash_set_zeros(hash);
		return;
	}

	hmac_sha256_for_data(hash, key, key_len, (const unsigned char *)str, strlen(str));
}

static void hmac_compute_key(const char *filename,
			     const struct update_stat *updt_stat,
			     char *key, size_t *key_len, bool use_xattrs)
{
	char *xattrs_blob = (void *)0xdeadcafe;
	size_t xattrs_blob_len = 0;

	if (use_xattrs) {
		xattrs_get_blob(filename, &xattrs_blob, &xattrs_blob_len);
	}

	hmac_sha256_for_data(key, (const unsigned char *)updt_stat,
			     sizeof(struct update_stat),
			     (const unsigned char *)xattrs_blob,
			     xattrs_blob_len);

	if (hash_is_zeros(key)) {
		*key_len = 0;
	} else {
		*key_len = SWUPD_HASH_LEN - 1;
	}

	if (xattrs_blob_len != 0) {
		free(xattrs_blob);
	}
}

void populate_file_struct(struct file *file, char *filename)
{
	struct stat stat;
	int ret;

	memset(&stat, 0, sizeof(stat));
	ret = lstat(filename, &stat);
	if (ret < 0) {
		LOG(NULL, "stat error ", "%s: %s", filename, strerror(errno));
		file->is_deleted = 1;
		return;
	}
	file->stat.st_mode = stat.st_mode;
	file->stat.st_uid = stat.st_uid;
	file->stat.st_gid = stat.st_gid;
	file->stat.st_rdev = stat.st_rdev;
	file->stat.st_size = stat.st_size;

	if (S_ISLNK(stat.st_mode)) {
		file->is_file = 0;
		file->is_dir = 0;
		file->is_link = 1;
		memset(&file->stat.st_mode, 0, sizeof(file->stat.st_mode));
		return;
	}

	if (S_ISDIR(stat.st_mode)) {
		file->is_file = 0;
		file->is_dir = 1;
		file->is_link = 0;
		file->stat.st_size = 0;
		return;
	}

	/* if we get here, this is a regular file */
	file->is_file = 1;
	file->is_dir = 0;
	file->is_link = 0;
	return;
}

/* this function MUST be kept in sync with the client
 * return is -1 if there was an error. If the file does not exist,
 * a "0000000..." hash is returned as is our convention in the manifest
 * for deleted files.  Otherwise file->hash is set to a non-zero hash. */
int compute_hash(struct file *file, char *filename)
{
	int ret;
	char key[SWUPD_HASH_LEN];
	size_t key_len;
	unsigned char *blob;
	FILE *fl;

	if (file->is_deleted) {
		hash_set_zeros(file->hash);
		return 0;
	}

	hash_set_zeros(key); /* Set to 64 '0' (not '\0') characters */

	if (file->is_link) {
		char link[PATH_MAX];
		memset(link, 0, PATH_MAX);

		ret = readlink(filename, link, PATH_MAX - 1);

		if (ret >= 0) {
			hmac_compute_key(filename, &file->stat, key, &key_len, file->use_xattrs);
			hmac_sha256_for_string(file->hash,
					       (const unsigned char *)key,
					       key_len,
					       link);
			return 0;
		} else {
			LOG(NULL, "readlink error ", "%i - %i / %s", ret, errno, strerror(errno));
			return -1;
		}
	}

	if (file->is_dir) {
		hmac_compute_key(filename, &file->stat, key, &key_len, file->use_xattrs);
		hmac_sha256_for_string(file->hash,
				       (const unsigned char *)key,
				       key_len,
				       SWUPD_HASH_DIRNAME); // Make independent of dirname
		return 0;
	}

	/* if we get here, this is a regular file */
	fl = fopen(filename, "r");
	if (!fl) {
		LOG(NULL, "file open error ", "%s: %s", filename, strerror(errno));
		return -1;
	}
	blob = mmap(NULL, file->stat.st_size, PROT_READ, MAP_PRIVATE, fileno(fl), 0);
	assert(!(blob == MAP_FAILED && file->stat.st_size != 0));

	hmac_compute_key(filename, &file->stat, key, &key_len, file->use_xattrs);
	hmac_sha256_for_data(file->hash,
			     (const unsigned char *)key,
			     key_len,
			     blob,
			     file->stat.st_size);
	munmap(blob, file->stat.st_size);
	fclose(fl);
	return 0;
}

static void get_hash(gpointer data, gpointer user_data)
{
	struct file *file = data;
	char *base = user_data;
	char *filename = NULL;
	int ret;

	if (base == NULL) {
		string_or_die(&filename, "%s", file->filename);
	} else {
		string_or_die(&filename, "%s/%s", base, file->filename);
	}
	assert(filename);

	file->use_xattrs = compute_hash_with_xattrs(filename);

	populate_file_struct(file, filename);
	ret = compute_hash(file, filename);
	if (ret != 0) {
		printf("Hash computation failed\n");
		assert(0);
	}

	free(filename);
}

/* disallow characters which can do unexpected things when the filename is
 * used on a tar command line via system("tar [args] filename [more args]");
 */
static bool illegal_characters(const char *filename)
{
	char c;
	int i;
#define BAD_CHAR_COUNT 11
	char bad_chars[BAD_CHAR_COUNT] = { ';', '&', '|', '*', '`', '/',
					   '<', '>', '\\', '\"', '\'' };

	// these breaks the tar transform sed-like expression,
	// hopefully can remove this check after moving to libtar
	if (strncmp(filename, "+", 1) == 0) {
		return true;
	}
	if (strstr(filename, "+package+") != NULL) {
		return true;
	}

	for (i = 0; i < BAD_CHAR_COUNT; i++) {
		c = bad_chars[i];
		if (strchr(filename, c) != NULL) {
			return true;
		}
	}
	return false;
}

static struct file *add_file(struct manifest *manifest,
			     const char *entry_name,
			     char *sub_filename,
			     char *fullname,
			     bool do_hash)
{
	GError *err = NULL;
	struct file *file;

	if (illegal_characters(entry_name)) {
		printf("WARNING: Filename %s includes illegal character(s) ...skipping.\n", sub_filename);
		free(sub_filename);
		free(fullname);
		return NULL;
	}

	file = calloc(1, sizeof(struct file));
	assert(file);

	file->last_change = manifest->version;
	file->filename = sub_filename;

	populate_file_struct(file, fullname);
	if (file->is_deleted) {
		/*
		 * populate_file_struct() logs a stat() failure, but
		 * does not abort. When adding files that should
		 * exist, this case is an error.
		 */
		LOG(NULL, "file not found", "%s", fullname);
		assert(0);
	}

	/* if for some reason there is a file in the official build
	 * which should not be included in the Manifest, then open a bug
	 * to get it removed, and work around its presence by
	 * excluding it here, eg:
	 if (strncmp(file->filename, "/dev/", 5) == 0) {
	 continue;
	 }
	*/

	if (do_hash) {
		/* compute the hash from a thread */
		int ret;
		ret = g_thread_pool_push(threadpool, file, &err);
		if (ret == FALSE) {
			printf("GThread hash computation push error\n");
			printf("%s\n", err->message);
			assert(0);
		}
	}
	manifest->files = g_list_prepend(manifest->files, file);
	manifest->count++;
	return file;
}

static void iterate_directory(struct manifest *manifest, char *pathprefix,
			      char *subpath, bool do_hash)
{
	DIR *dir;
	struct dirent *entry;
	char *fullpath;

	string_or_die(&fullpath, "%s/%s", pathprefix, subpath);

	dir = opendir(fullpath);
	if (!dir) {
		bool fatal_error = errno != ENOENT;
		FILE *content;

		free(fullpath);
		if (fatal_error) {
			return;
		}
		/*
		 * If there is a <dir>.content.txt instead of
		 * the actual directory, then read that
		 * file. It has a list of path names,
		 * including all directories. The
		 * corresponding file system entry is then
		 * expected to be in a pre-populated "full"
		 * directory.
		 *
		 * Only supported at top level (i.e. empty
		 * subpath) to keep the code and testing
		 * simpler.
		 */
		assert(!subpath[0]);
		string_or_die(&fullpath, "%s.content.txt", pathprefix);
		content = fopen(fullpath, "r");
		free(fullpath);
		fullpath = NULL;
		if (content) {
			char *line = NULL;
			size_t len = 0;
			ssize_t read;
			const char *full;
			int full_len;
			/*
			 * determine path to "full" directory: it is assumed to be alongside
			 * "pathprefix", i.e. pathprefix/../full. But pathprefix does not exit,
			 * so we have to strip the last path component.
			 */
			full = strrchr(pathprefix, '/');
			if (full) {
				full_len = full - pathprefix + 1;
				full = pathprefix;
			} else {
				full = "";
				full_len = 0;
			}
			while ((read = getline(&line, &len, content)) != -1) {
				if (read) {
					const char *entry_name = strrchr(line, '/');
					if (entry_name) {
						entry_name++;
					} else {
						entry_name = line;
					}
					if (line[read - 1] == '\n') {
						line[read - 1] = 0;
					}
					string_or_die(&fullpath, "%.*sfull/%s", full_len, full, line);
					add_file(manifest,
						 entry_name,
						 strdup(line),
						 fullpath,
						 do_hash);
				}
			}
			free(line);
		}

		// If both directory and content file are missing, silently (?)
		// don't add anything to the manifest.
		return;
	}

	while (dir) {
		char *sub_filename;
		char *fullname;
		struct file *file;

		entry = readdir(dir);
		if (!entry) {
			break;
		}

		if ((strcmp(entry->d_name, ".") == 0) ||
		    (strcmp(entry->d_name, "..") == 0)) {
			continue;
		}

		string_or_die(&sub_filename, "%s/%s", subpath, entry->d_name);
		string_or_die(&fullname, "%s/%s", fullpath, entry->d_name);

		/* takes ownership of the strings, so we don't need to free it */
		file = add_file(manifest, entry->d_name, sub_filename, fullname, do_hash);

		if (file && file->is_dir) {
			iterate_directory(manifest, pathprefix, file->filename, do_hash);
		}
	}
	closedir(dir);
	free(fullpath);
}

struct manifest *full_manifest_from_directory(int version)
{
	struct manifest *manifest;
	char *dir;
	int numthreads = num_threads(1.0);

	LOG(NULL, "Computing hashes", "for %i/full", version);

	manifest = alloc_manifest(version, "full", NULL);

	string_or_die(&dir, "%s/%i/full", image_dir, version);

	threadpool = g_thread_pool_new(get_hash, dir, numthreads, FALSE, NULL);

	iterate_directory(manifest, dir, "", true);

	/* wait for the hash computation to finish */
	g_thread_pool_free(threadpool, FALSE, TRUE);
	free(dir);

	manifest->files = g_list_sort(manifest->files, file_sort_filename);

	return manifest;
}

/* Read includes from $manifest-includes file */
GList *get_sub_manifest_includes(char *component, int version)
{
	FILE *infile;
	GList *includes = NULL;
	char *c;
	char *conf;
	char *filename;
	char *included;
	char line[8192];

	conf = config_image_base();
	if (conf == NULL) {
		assert(0);
	}

	string_or_die(&filename, "%s/%i/noship/%s-includes", conf, version, component);
	free(conf);

	LOG(NULL, "Reading includes", "%s", filename);
	infile = fopen(filename, "rb");

	if (infile == NULL) {
		if (errno != ENOENT) {
			LOG(NULL, "Cannot read includes", "%s (%s)\n", filename, strerror(errno));
		}
		free(filename);
		return NULL;
	}

	line[0] = 0;
	while (strcmp(line, "\n") != 0) {
		line[0] = 0;
		if (fgets(line, 8191, infile) == NULL) {
			break;
		}
		c = strchr(line, '\n');
		if (c) {
			*c = 0;
		}
		if (strlen(line) == 0) {
			break;
		}
		included = strdup(line);
		includes = g_list_prepend(includes, included);
	}
	fclose(infile);
	includes = g_list_sort(includes, (GCompareFunc)strcmp);

	return includes;
}

struct manifest *sub_manifest_from_directory(char *component, int version)
{
	struct manifest *manifest;
	char *dir;

	LOG(NULL, "Creating component manifest", "for %i/%s", version, component);

	manifest = alloc_manifest(version, component, NULL);

	string_or_die(&dir, "%s/%i/%s", image_dir, version, component);

	iterate_directory(manifest, dir, "", false);

	free(dir);

	manifest->files = g_list_sort(manifest->files, file_sort_filename);

	manifest->includes = get_sub_manifest_includes(component, version);

	return manifest;
}

/* get hashes out of full manifest and add them into the component manifest */
void add_component_hashes_to_manifest(struct manifest *compm, struct manifest *fullm)
{
	GList *list1, *list2;
	struct file *file1, *file2;
	int ret;

	assert(compm);
	assert(fullm);

	compm->files = g_list_sort(compm->files, file_sort_filename);
	fullm->files = g_list_sort(fullm->files, file_sort_filename);

	list1 = g_list_first(compm->files);
	list2 = g_list_first(fullm->files);

	while (list1 && list2) {
		file1 = list1->data;
		file2 = list2->data;

		ret = strcmp(file1->filename, file2->filename);

		if (file2->is_deleted) {
			list2 = g_list_next(list2);
			continue;
		}

		if (ret == 0) {
			hash_assign(file2->hash, file1->hash);
			list1 = g_list_next(list1);
			list2 = g_list_next(list2);
		} else if (ret < 0) {
			list1 = g_list_next(list1);
		} else {
			list2 = g_list_next(list2);
		}
	}
}
