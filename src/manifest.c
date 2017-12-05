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
 *         tkeel <thomas.keel@intel.com>
 *
 */

#define _GNU_SOURCE
#include <assert.h>
#include <bsdiff.h>
#include <errno.h>
#include <glib.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "swupd.h"
#include "xattrs.h"

#define LINK_SIZE_HINT 1024
#define DIR_SIZE_HINT 1024

int file_sort_hash(gconstpointer a, gconstpointer b)
{
	struct file *A, *B;
	A = (struct file *)a;
	B = (struct file *)b;

	/* this MUST be a memcmp(), not bcmp() */
	return memcmp(A->hash, B->hash, SWUPD_HASH_LEN - 1);
}

/* Standard sort compare function which sorts
 * first on the version where the file changed and then
 * on the name
 */
int file_sort_version(gconstpointer a, gconstpointer b)
{
	struct file *A, *B;
	A = (struct file *)a;
	B = (struct file *)b;

	if (A->last_change < B->last_change) {
		return -1;
	}
	if (A->last_change > B->last_change) {
		return 1;
	}

	return strcmp(A->filename, B->filename);
}

int file_sort_filename(gconstpointer a, gconstpointer b)
{
	struct file *A, *B;
	int ret;
	A = (struct file *)a;
	B = (struct file *)b;

	ret = strcmp(A->filename, B->filename);
	if (ret) {
		return ret;
	}
	if (A->is_deleted > B->is_deleted) {
		return 1;
	}
	if (A->is_deleted < B->is_deleted) {
		return -1;
	}

	return 0;
}

struct manifest *alloc_manifest(int version, char *component, GList *actions)
{
	struct manifest *manifest;

	manifest = calloc(1, sizeof(struct manifest));
	if (manifest == NULL) {
		assert(0);
	}

	manifest->version = version;
	manifest->component = strdup(component);
	manifest->format = format;
	manifest->actions = actions;

	return manifest;
}

struct manifest *manifest_from_file(int version, char *component)
{
	FILE *infile;
	GList *includes = NULL;
	char line[8192], *c, *c2;
	int count = 0;
	struct manifest *manifest;
	char *filename, *conf;
	int previous = 0;
	unsigned long long int format_number;

	conf = config_output_dir();
	if (conf == NULL) {
		assert(0);
	}

	string_or_die(&filename, "%s/%i/Manifest.%s", conf, version, component);
	free(conf);

	LOG(NULL, "Reading manifest", "%s", filename);
	infile = fopen(filename, "rb");

	if (infile == NULL) {
		LOG(NULL, "Cannot read manifest", "%s (%s)\n", filename, strerror(errno));
		free(filename);
		return alloc_manifest(version, component, NULL);
	}

	/* line 1: MANIFEST\t<version> */
	line[0] = 0;
	if (fgets(line, 8191, infile) == NULL) {
		fclose(infile);
		return NULL;
	}

	if (strncmp(line, "MANIFEST\t", 9) != 0) {
		printf("Invalid file format: MANIFEST line\n");
		fclose(infile);
		return NULL;
	}
	c = &line[9];
	format_number = strtoull(c, NULL, 10);
	if ((errno < 0) || (format_number == 0)) {
		//format string shall be a positive integer
		printf("Unknown file format version in MANIFEST line: %s\n", c);
		fclose(infile);
		return NULL;
	}
	line[0] = 0;
	while (strcmp(line, "\n") != 0) {
		/* read the header */
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
		c = strchr(line, '\t');
		/* Make sure we're not at the end of the array before incrementing */
		if (c && c <= &line[strlen(line)]) {
			c++;
		} else {
			printf("Manifest is corrupt\n");
			assert(0);
		}

		if (strncmp(line, "version:", 8) == 0) {
			version = strtoull(c, NULL, 10);
		}
		if (strncmp(line, "previous:", 9) == 0) {
			previous = strtoull(c, NULL, 10);
		}
		if (strncmp(line, "includes:", 9) == 0) {
			includes = g_list_prepend(includes, strdup(c));
			if (!includes->data) {
				abort();
			}
		}
	}

	manifest = alloc_manifest(version, component, NULL);
	manifest->format = format_number;
	manifest->prevversion = previous;
	manifest->includes = includes;

	/* empty line */
	while (!feof(infile)) {
		struct file *file;

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

		file = calloc(1, sizeof(struct file));
		if (file == NULL) {
			assert(0);
		}
		c = line;

		c2 = strchr(c, '\t');
		if (c2) {
			*c2 = 0;
			c2++;
		}

		if (c[0] == 'F') {
			file->is_file = 1;
		} else if (c[0] == 'D') {
			file->is_dir = 1;
		} else if (c[0] == 'L') {
			file->is_link = 1;
		} else if (c[0] == 'M') {
			LOG(NULL, "Found a manifest!", "%s", c);
			file->is_manifest = 1;
		} else if (c[0] != '.') {
			assert(0); /* unknown file type */
		}

		switch (c[1]) {
		case 'd':
			/* file is deleted */
			file->is_deleted = 1;
			break;
		case 'g':
			/* file is ghosted */
			file->is_ghosted = 1;
			break;
		case '.':
			/* no flag at this index */
			break;
		default:
			/* unknown deleted status */
			LOG(NULL, "Invalid flag at index 1", "%s", c[1]);
			assert(0);
		}

		if (c[2] == 'C') {
			file->is_config = 1;
		} else if (c[2] == 's') {
			file->is_state = 1;
		} else if (c[2] == 'b') {
			file->is_boot = 1;
		} else if (c[2] != '.') {
			assert(0); /* unknown modifier status */
		}

		if (c[3] == 'r') {
			file->is_rename = 1;
		} else if (c[3] != '.') {
			; /* field 4: ignore unknown letters */
		}

		c = c2;
		if (!c) {
			free(file);
			continue;
		}
		c2 = strchr(c, '\t');
		if (c2) {
			*c2 = 0;
			c2++;
		}

		hash_assign(c, file->hash);

		c = c2;
		if (!c) {
			free(file);
			continue;
		}
		c2 = strchr(c, '\t');
		if (c2) {
			*c2 = 0;
			c2++;
		}

		file->last_change = strtoull(c, NULL, 10);

		c = c2;
		if (!c) {
			free(file);
			continue;
		}
		file->filename = strdup(c);

		if (file->is_manifest) {
			nest_manifest_file(manifest, file);
		} else {
			manifest->files = g_list_prepend(manifest->files, file);
		}
		manifest->count++;
		count++;
	}

	manifest->files = g_list_sort(manifest->files, file_sort_filename);
	fclose(infile);
	LOG(NULL, "Manifest info", "Manifest for version %i/%s contains %i files", version, component, count);
	free(filename);
	return manifest;
}

/* do not set peer for the case where the old file was deleted and the new file
 * was added back. This peer is normally set when the names match up and this
 * triggers an attempt at creating a delta during pack creation. For this case
 * where the old file is deleted or ghosted and is brought back we do not want
 * this to be attempted, as the old file will not be present when trying to
 * create the delta.
 *
 * We can go even further and not set peers for files when the old one did not
 * exist (deleted or ghosted). It is not necessary to check the status of the
 * new file */
bool should_have_peer(struct file *file1, struct file *file2)
{
	return (!(file1->is_deleted || file1->is_ghosted));
}

void free_manifest(struct manifest *manifest)
{
	struct file *file;

	if (!manifest) {
		return;
	}

	while (manifest->files) {
		file = manifest->files->data;
		free(file->filename);
		free(file);
		manifest->files = g_list_delete_link(manifest->files, manifest->files);
	}
	free(manifest);
}

static bool same_file_contents(struct file *file1, struct file *file2)
{
	return (file1->is_dir == file2->is_dir &&
		file1->is_link == file2->is_link &&
		file1->is_deleted == file2->is_deleted &&
		file1->is_file == file2->is_file &&
		file1->is_config == file2->is_config &&
		file1->is_state == file2->is_state &&
		file1->is_boot == file2->is_boot &&
		hash_compare(file1->hash, file2->hash));
}

/*
 * Add a deleted file entry for it in the target list. However, since we're
 * currently walking the list we HAVE to prepend the entry. Calling function
 * should track to sort at the end.
 */
static void add_deleted_file(struct file *source, struct manifest *manifest)
{
	struct file *deleted;
	deleted = calloc(1, sizeof(struct file));
	if (deleted == NULL) {
		assert(0);
	}

	deleted->filename = strdup(source->filename);
	hash_set_zeros(deleted->hash);
	deleted->is_deleted = 1;
	deleted->is_config = source->is_config;
	deleted->is_state = source->is_state;
	deleted->is_boot = source->is_boot;
	/* ghost deleted boot files */
	deleted->is_ghosted = source->is_ghosted | source->is_boot & source->is_deleted;

	if (deleted->is_ghosted || source->is_deleted) {
		/* if the new file is ghosted or the file was deleted, preserve
		 * hash (all zeros if source->is_deleted) and rename status */
		deleted->is_rename = source->is_rename;
		hash_assign(source->hash, deleted->hash);
	}

	/* for deleted files last_change remains the same.
	 * otherwise last change is now */
	deleted->last_change = source->is_deleted ? source->last_change : manifest->version;

	deleted->peer = source;
	source->peer = deleted;

	/* if we are adding a deleted file we are walking the old and new
	 * manifest files in-sync. we need to prepend this file in order to
	 * not process it twice. This is why it is important for the calling
	 * function to sort the list again at the end */
	manifest->files = g_list_prepend(manifest->files, deleted);
	manifest->count++;
}

/*
backfill the "last changed" of each file in a manifest
by comparing the hash against the same file in the previous manifest

in theory this is a O(N^2) operation, but due to sorting and new files being rare,
this is more like O(2N) in practice.
*/

int match_manifests(struct manifest *m1, struct manifest *m2)
{
	GList *list1, *list2;
	struct file *file1, *file2;
	int must_sort = 0;
	int count = 0;

	if (!m1) {
		printf("Matching manifests up failed: No old manifest!\n");
		return -1;
	}

	if (!m2) {
		printf("Matching manifests up failed: No new manifest!\n");
		return -1;
	}

	m1->files = g_list_sort(m1->files, file_sort_filename);
	m2->files = g_list_sort(m2->files, file_sort_filename);

	list1 = g_list_first(m1->files);
	list2 = g_list_first(m2->files);

	while (list1 && list2) {
		int ret;
		file1 = list1->data;
		file2 = list2->data;

		file1->peer = NULL;
		file2->peer = NULL;

		ret = strcmp(file1->filename, file2->filename);
		if (ret == 0) {
			/* file is present in both manifests */
			if (same_file_contents(file1, file2) && file1->last_change >= minversion) {
				/* file did not change */
				file2->last_change = file1->last_change;
				file2->is_rename = file1->is_rename;
			} else {
				/* file changed */
				account_changed_file();
				count++;
			}

			/* check if these files should be peers */
			if (should_have_peer(file1, file2)) {
				file1->peer = file2;
				file2->peer = file1;
			}

			/* there was a match, advance both lists */
			list1 = g_list_next(list1);
			list2 = g_list_next(list2);
		} else if (ret < 0) {
			/* file1 was deleted, create entry for deleted file */
			add_deleted_file(file1, m2);
			if (!file1->is_deleted) {
				account_deleted_file();
				count++;
			}

			must_sort = 1;
			/* advance list1 for next file */
			list1 = g_list_next(list1);
		} else {
			/* if we get here, ret is > 0, which means this is a new file added */
			/* all we do is advance the pointer */
			account_new_file();
			count++;
			/* advance list2 to check against same file in list1 */
			list2 = g_list_next(list2);
		}
	}

	/* now deal with the tail ends */
	/* deleted files from list1 */
	for (; list1; list1 = g_list_next(list1)) {
		file1 = list1->data;
		add_deleted_file(file1, m2);
		if (!file1->is_deleted) {
			account_deleted_file();
			count++;
		}

		must_sort = 1;
	}

	/* added files from list2 */
	for (; list2; list2 = g_list_next(list2)) {
		account_new_file();
		count++;
	}

	/* finally, sort the list if necessary */
	if (must_sort) {
		m2->files = g_list_sort(m2->files, file_sort_filename);
	}

	/* returned count of changed files */
	return count;
}

static GList *get_unique_includes(struct manifest *manifest)
{
	GHashTable *unique_includes = g_hash_table_new(g_str_hash, g_str_equal);
	GHashTableIter iter;
	GList *includes = NULL;
	GList *l1, *l2;
	gpointer k, v;

	l1 = g_list_first(manifest->includes);
	while (l1) {
		struct manifest *m = l1->data;
		l1 = g_list_next(l1);
		(void)g_hash_table_replace(unique_includes, m->component, m);
		l2 = get_unique_includes(m);
		while (l2) {
			struct manifest *m = l2->data;
			l2 = g_list_next(l2);
			(void)g_hash_table_replace(unique_includes, m->component, m);
		}
		g_list_free(l2);
	}
	g_hash_table_iter_init(&iter, unique_includes);
	while (g_hash_table_iter_next(&iter, &k, &v)) {
		includes = g_list_prepend(includes, v);
	}

	return includes;
}

/*
 * removes all files from m2 from m1
 * This will recurse over all included manifests to also
 * subtract those from the m1 manifest.
 *
 * As a special convenient semantics,
 * subtract_manifests(M, M)
 * will subtract all included manifests from M,
 * but will not subtract M from M itself.
 */
void subtract_manifests(struct manifest *m1, struct manifest *m2)
{
	GList *list1, *list2;
	struct file *file1, *file2;

	m1->files = g_list_sort(m1->files, file_sort_filename);
	m2->files = g_list_sort(m2->files, file_sort_filename);

	list1 = g_list_first(m1->files);
	list2 = g_list_first(m2->files);

	while (list1 && list2 && m1 != m2) {
		int ret;
		file1 = list1->data;
		file2 = list2->data;

		ret = strcmp(file1->filename, file2->filename);
		if (ret == 0) {
			GList *todel;
			todel = list1;
			list1 = g_list_next(list1);
			list2 = g_list_next(list2);

			/* When both files are marked deleted, skip
			 * subtraction. Preserving the deleted entries in both
			 * manifests is required for 'swupd update' to know
			 * when to delete the file, because the m2 bundle may
			 * be installed with or without the m1 bundle.
			 */
			if (file1->is_deleted && file2->is_deleted) {
				continue;
			}

			if (file1->is_deleted == file2->is_deleted && file1->is_file == file2->is_file) {
				m1->files = g_list_delete_link(m1->files, todel);
				m1->count--;
			}
		} else if (ret < 0) {
			list1 = g_list_next(list1);
		} else {
			list2 = g_list_next(list2);
		}
	}
}

void subtract_manifests_frontend(struct manifest *m1, struct manifest *m2)
{
	GList *includes;
	struct manifest *m;

	if (!m1) {
		printf("Subtracting manifests failed: No m1 manifest!\n");
		return;
	}

	if (!m2) {
		printf("Subtracting manifests failed: No m2 manifest!\n");
		return;
	}

	subtract_manifests(m1, m2);

	includes = get_unique_includes(m2);
	while (includes) {
		m = includes->data;
		includes = g_list_next(includes);
		subtract_manifests(m1, m);
	}
}

char *file_type_to_string(struct file *file)
{
	static char type[5];

	strcpy(type, "....");

	if (file->is_dir) {
		type[0] = 'D';
	}

	if (file->is_link) {
		type[0] = 'L';
	}
	if (file->is_file) {
		type[0] = 'F';
	}
	if (file->is_manifest) {
		type[0] = 'M';
	}

	if (file->is_deleted) {
		type[1] = 'd';
	}

	if (file->is_ghosted) {
		type[1] = 'g';
	}

	if (file->is_config) {
		type[2] = 'C';
	}
	if (file->is_state) {
		type[2] = 's';
	}
	if (file->is_boot) {
		type[2] = 'b';
	}

	if (file->is_rename) {
		type[3] = 'r';
	}

	return type;
}

/* Calculate the contentsize for the manifest based on file sizes.
 *
 * This should calculate the files uniquely included in this manifest, but none
 * of its submanifests, which will allow calculation of sizes of all bundles on
 * a system by adding all manifest->contentsizes of installed bundles.
 *
 * However, if two bundles not in the same include chain have overlapping
 * content, summing the include chain of each bundle in the client will result
 * in an over-estimation of the total size on the system. The more content is
 * shared, the higher the over-estimation. In reality this overlap will not be
 * large, but it is currently impossible to calculate the exact installed size
 * using just the contentsize.
 */
static void compute_content_size(struct manifest *manifest)
{
	GList *list;
	struct file *file;

	list = g_list_first(manifest->files);
	while (list) {
		file = list->data;
		list = g_list_next(list);
		if (!file->is_deleted) {
			if (file->is_file) {
				manifest->contentsize += file->stat.st_size;
			} else if (file->is_link) {
				manifest->contentsize += LINK_SIZE_HINT;
			} else if (file->is_dir) {
				manifest->contentsize += DIR_SIZE_HINT;
			}
		}
	}
}

/* Returns 0 == success, -1 == failure */
static int write_manifest_plain(struct manifest *manifest)
{
	GList *includes;
	GList *list;
	GList *actions;
	struct file *file;
	FILE *out = NULL;
	char *base = NULL, *dir;
	char *conf = config_output_dir();
	char *filename = NULL;
	char *status = NULL;
	char *submanifest_filename = NULL;
	char *manifest_tempdir = NULL;
	char *tempmanifest = NULL;
	int ret = -1;

	if (conf == NULL) {
		assert(0);
	}
	string_or_die(&filename, "%s/%i/Manifest.%s", conf, manifest->version, manifest->component);

	base = strdup(filename);
	if (base == NULL) {
		assert(0);
	}
	dir = dirname(base);

	if (g_mkdir_with_parents(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
		assert(0);
	}

	out = fopen(filename, "w");
	if (out == NULL) {
		printf("Failed to open %s for write\n", filename);
		goto exit;
	}

	fprintf(out, "MANIFEST\t%llu\n", format);
	fprintf(out, "version:\t%i\n", manifest->version);
	fprintf(out, "previous:\t%i\n", manifest->prevversion);
	fprintf(out, "filecount:\t%i\n", manifest->count);
	fprintf(out, "timestamp:\t%i\n", (int)time(NULL));
	compute_content_size(manifest);
	fprintf(out, "contentsize:\t%llu\n", (long long unsigned int)manifest->contentsize);
	includes = manifest->includes;
	status = group_status(manifest->component);
	if (status) {
		fprintf(out, "status:\t%s\n", status);
	}
	while (includes) {
		struct manifest *sub = includes->data;
		includes = g_list_next(includes);
		fprintf(out, "includes:\t%s\n", sub->component);
	}

	actions = manifest->actions;
	while (actions) {
		char *action = actions->data;
		fprintf(out, "actions:\t%s\n", action);
		actions = g_list_next(actions);
	}

	fprintf(out, "\n");

	list = g_list_first(manifest->files);
	while (list) {
		file = list->data;
		list = g_list_next(list);

		fprintf(out, "%s\t%s\t%i\t%s\n", file_type_to_string(file), file->hash, file->last_change, file->filename);
	}

	list = g_list_first(manifest->manifests);
	string_or_die(&manifest_tempdir, "%s/XXXXXX", state_dir);
	if (!mkdtemp(manifest_tempdir)) {
		LOG(NULL, "Failed to create temporary directory %s for untarring Manifests", manifest_tempdir);
		assert(0);
	}

	while (list) {
		file = list->data;
		list = g_list_next(list);

		string_or_die(&submanifest_filename, "%s/%i/Manifest.%s", conf, file->last_change, file->filename);
		populate_file_struct(file, submanifest_filename);

		if (file->is_deleted) {
			goto write_entry;
		}

		char *param1, *param2;

		/* Untar the Manifest.BUNDLE.tar and calculate the hash on that,
		 * otherwise we may get incorrect hashes due to owner permissions on
		 * the files. */
		string_or_die(&param1, "%s", manifest_tempdir);
		string_or_die(&param2, "%s.tar", submanifest_filename);
		char *const tarcmd[] = { TAR_COMMAND, "-C", param1, TAR_PERM_ATTR_ARGS_STRLIST, "-xf", param2, NULL };
		ret = system_argv(tarcmd);
		free(param1);
		free(param2);

		if (ret != 0) {
			assert(0);
		}

		string_or_die(&tempmanifest, "%s/Manifest.%s", manifest_tempdir, file->filename);
		populate_file_struct(file, tempmanifest);
		ret = compute_hash(file, tempmanifest);
		if (ret != 0) {
			printf("Hash computation failed\n");
			assert(0);
		}
		unlink(tempmanifest);
		free(tempmanifest);
	write_entry:
		fprintf(out, "%s\t%s\t%i\t%s\n", file_type_to_string(file), file->hash, file->last_change, file->filename);
		free(submanifest_filename);
	}

	ret = 0;
exit:
	if (manifest_tempdir) {
		if (rmdir(manifest_tempdir) != 0) {
			LOG(NULL, "rmdir failed for %s: %s", manifest_tempdir, strerror(errno));
		}
		free(manifest_tempdir);
	}
	if (out) {
		fclose(out);
	}
	free(conf);
	free(base);
	free(filename);
	return ret;
}

/* Returns 0 == success, -1 == failure */
static int write_manifest_tar(struct manifest *manifest)
{
	char *conf = config_output_dir();
	char *directory, *manifesttar, *manifestcomp;
	int ret = 0;

	if (conf == NULL) {
		assert(0);
	}

	string_or_die(&directory, "--directory=%s/%i", conf, manifest->version);
	string_or_die(&manifesttar, "%s/%i/Manifest.%s.tar", conf, manifest->version, manifest->component);
	string_or_die(&manifestcomp, "Manifest.%s", manifest->component);

	/* now, tar the thing up for efficient full file download */
	char *const tarcmd[] = { TAR_COMMAND, directory, TAR_PERM_ATTR_ARGS_STRLIST, "-Jcf",
				 manifesttar, manifestcomp, NULL };
	ret = system_argv(tarcmd);
	if (ret) {
		fprintf(stderr, "Creation of Manifest.tar failed\n");
	}

	free(directory);
	free(manifesttar);
	free(manifestcomp);
	free(conf);
	return ret;
}

bool create_download_content_for_group(const char *group)
{
#warning should find a generic way to exclude some groups to pack creation
	if (strcmp(group, "esp")) {
		return true;
	}

	return false;
}

bool compute_hash_with_xattrs(const char *filename)
{
#warning should find a generic way to specify if we should not use xattrs for calculating the hashes of some groups
	if (strncmp(filename, "/boot/", 6) == 0) {
		return true;
	}

	/* Currently swupd-client includes xattrs to hash sums of all files */
	return true;
}

/* Returns 0 == success, -1 == failure */
int write_manifest(struct manifest *manifest)
{
	if (write_manifest_plain(manifest) == 0 &&
	    write_manifest_tar(manifest) == 0) {
		return 0;
	}
	return -1;
}

void sort_manifest_by_version(struct manifest *manifest)
{
	if (manifest->files) {
		manifest->files = g_list_sort(manifest->files, file_sort_version);
	}
	if (manifest->manifests) {
		manifest->manifests = g_list_sort(manifest->manifests, file_sort_version);
	}
}

bool manifest_includes(struct manifest *manifest, char *component)
{
	GList *includes = g_list_first(manifest->includes);

	while (includes) {
		if (strcmp(((struct manifest *)includes->data)->component,
			   component) == 0) {
			return true;
		}
		includes = g_list_next(includes);
	}

	return false;
}

/* This requires the manifest to have the includes sorted.
 * The manifests will be sorted when constructed from
 * get_sub_manifest_includes (and then replacing the manifest
 * strings with actual manifest structs in the same order as
 * is done in the main.c's main()).
 */
bool changed_includes(struct manifest *old, struct manifest *new)
{
	GList *includes_old = g_list_first(old->includes);
	GList *includes_new = g_list_first(new->includes);

	if (g_list_length(includes_old) != g_list_length(includes_new)) {
		return true;
	}

	while (includes_old && includes_new) {
		char *bundle_old = ((struct manifest *)includes_old->data)->component;
		char *bundle_new = ((struct manifest *)includes_new->data)->component;
		if (strcmp(bundle_old, bundle_new) != 0) {
			return true;
		}

		includes_old = g_list_next(includes_old);
		includes_new = g_list_next(includes_new);
	}

	return false;
}

/* Remove files deprecated in this version as identified by the compfunc
 * function. This function can be used to remove old deleted files over a format
 * bump and remove deprecated ghosted files.
 * Note: this function should be called after match_manifests().
 */
int remove_deprecated_files(struct manifest *m1, struct manifest *m2, bool (*compfunc)(struct file *file1, struct file *file2))
{
	GList *list1, *list2;
	struct file *file1, *file2;
	int count = 0;

	if (!m1) {
		printf("No old manifest!\n");
		return -1;
	}

	if (!m2) {
		printf("No new manifest!\n");
		return -1;
	}

	/* At this point, the manifest formats mismatch, and it's assumed that
	 * m1 is the old manifest, and m2 is the new.
	 */

	m1->files = g_list_sort(m1->files, file_sort_filename);
	m2->files = g_list_sort(m2->files, file_sort_filename);

	list1 = g_list_first(m1->files);
	list2 = g_list_first(m2->files);

	while (list1 && list2) {
		int ret;
		file1 = list1->data;
		file2 = list2->data;

		ret = strcmp(file1->filename, file2->filename);
		if (ret == 0) {
			/* use the comparison function passed in to determine if this file
			 * should be removed */
			if (compfunc(file1, file2)) {
				GList *to_delete = list2;
				list1 = g_list_next(list1);
				list2 = g_list_next(list2);
				m2->files = g_list_delete_link(m2->files, to_delete);
				m2->count--;
				count++;
				continue;
			}
			list1 = g_list_next(list1);
			list2 = g_list_next(list2);
		} else if (ret < 0) {
			list1 = g_list_next(list1);
		} else {
			list2 = g_list_next(list2);
		}
	}

	return count;
}

/* Performs a final link of renames in the manifest to catch any renames from
 * old versions and then removes any orphaned renames */
void clean_renames(struct manifest *manifest)
{
	GList *list;
	struct file *file;

	/* make sure all renames are linked, this is necessary for renames
	 * from old manifests that carry over to the current one */
	final_link(manifest->files);

	list = g_list_first(manifest->files);
	while (list) {
		file = list->data;
		/* if a file is marked as a rename but has lost its rename_peer
		 * it needs to be cleaned up */
		if (file->is_rename && !file->rename_peer) {
			/* no longer a rename */
			file->is_rename = 0;
			/* if the file is marked as deleted and renamed it is a
			 * renamed-from file. Mark these as deleted now */
			if (file->is_deleted) {
				hash_set_zeros(file->hash);
			}
		}

		list = g_list_next(list);
	}
}

/* Conditionally remove some things from a manifest.
 * Returns > 0 when the pruned manifest has new files.
 * Returns 0 when the pruned manifest no longer has new files.
 * Returns -1 on error.
 */
int prune_manifest(struct manifest *manifest)
{
	GList *list;
	GList *next;
	struct file *file;
	int newfiles = 0;

	/* prune some files */
	list = g_list_first(manifest->files);
	while (list) {
		next = g_list_next(list);
		file = list->data;

		if (OS_IS_STATELESS && (!file->is_deleted) && (file->is_config)) {
			// toward being a stateless OS
			LOG(file, "Skipping config file in manifest write", "component %s", manifest->component);
			manifest->files = g_list_delete_link(manifest->files, list);
			manifest->count--;
		} else if (file->is_boot && file->is_deleted) {
			/* mark boot files that are going away as ghosted, these will be
			 * cleaned up with the next update */
			file->is_deleted = 0;
			file->is_ghosted = 1;
		} else if (config_ban_debuginfo() && file_is_debuginfo(file->filename)) {
			/* The configuration option to ban debuginfo from the manifests was
			 * set in server.ini via the [Debuginfo][banned] option. Although
			 * debuginfo additions are banned via analyze_fs, prune it here
			 * to insure mistakenly included debuginfo from old versions is
			 * removed from the manifests. */
			manifest->files = g_list_delete_link(manifest->files, list);
			manifest->count--;
		}
		list = next;
	}

	/* check we haven't pruned all the new files */
	list = g_list_first(manifest->files);
	while (list) {
		next = g_list_next(list);
		file = list->data;
		list = next;

		if (file->last_change == manifest->version) {
			newfiles++;
		}
	}
	return newfiles;
}

void create_manifest_delta(int oldversion, int newversion, char *module)
{
	char *original = NULL, *newfile = NULL, *outfile = NULL, *dotfile = NULL;
	char *conf = config_output_dir();
	int ret;

	if (!conf) {
		assert(0);
	}

	string_or_die(&newfile, "%s/%i/Manifest.%s", conf, newversion, module);
	string_or_die(&original, "%s/%i/Manifest.%s", conf, oldversion, module);

	if (access(newfile, R_OK) != 0 || access(original, R_OK) != 0) {
		goto exit;
	}

	string_or_die(&outfile, "%s/%i/Manifest-%s-delta-from-%i", conf, newversion, module, oldversion);
	string_or_die(&dotfile, "%s/%i/.Manifest-%s-delta-from-%i", conf, newversion, module, oldversion);

	ret = xattrs_compare(original, newfile);
	if (ret != 0) {
		LOG(NULL, "xattrs have changed, don't create diff ", "%s", newfile);
		sleep(1);
	} else if (make_bsdiff_delta(original, newfile, dotfile, 0) == 0) {
		if (rename(dotfile, outfile) != 0) {
			if (errno == ENOENT) {
				LOG(NULL, "dotfile:", " %s does not exist", dotfile);
			}
			LOG(NULL, "Failed to rename", "");
		}
	} else {
		sleep(1); /* we raced. whatever. sleep for a bit to get the other guy to make progress */
	}

exit:
	free(conf);
	free(newfile);
	free(original);
	free(outfile);
	free(dotfile);
}

void create_manifest_deltas(struct manifest *manifest, GList *last_versions_list)
{
	GList *item;
	int prev_version;

	LOG(NULL, "Creating manifest deltas", "%d %s", manifest->version, manifest->component);
	/* Create manifest deltas from the previous manifests */
	item = g_list_first(last_versions_list);
	while (item) {
		prev_version = GPOINTER_TO_INT(item->data);
		item = g_list_next(item);
		create_manifest_delta(prev_version, manifest->version, manifest->component);
	}
	LOG(NULL, "Done creating manifest deltas", "");
}

/* adds a struct file for a submanifest and also adds it to the proper list */
void nest_manifest(struct manifest *parent, struct manifest *sub)
{
	struct file *file;

	file = calloc(1, sizeof(struct file));
	if (file == NULL) {
		assert(0);
	}

	file->last_change = sub->version;
	hash_set_zeros(file->hash);
	file->is_manifest = 1;
	file->filename = strdup(sub->component);

	parent->manifests = g_list_prepend(parent->manifests, file);
	parent->submanifests = g_list_prepend(parent->submanifests, sub);
	parent->count++;
}

/* adds a struct file for a submanifest and also adds it to the proper list */
void nest_manifest_file(struct manifest *parent, struct file *file)
{
	struct manifest *sub;

	sub = manifest_from_file(file->last_change, file->filename);

	parent->manifests = g_list_prepend(parent->manifests, file);
	parent->submanifests = g_list_prepend(parent->submanifests, sub);
	parent->count++;

	LOG(file, "Nest manifest file", "%s", file->filename);
}

int manifest_subversion(struct manifest *parent, char *group)
{
	GList *list;
	struct file *file;

	list = parent->manifests;
	while (list) {
		file = list->data;
		list = g_list_next(list);

		if (strcmp(file->filename, group) == 0) {
			return file->last_change;
		}
	}
	LOG(NULL, "No sub package found, returning 0", "");
	return 0;
}

static void maximize_version_manifests(struct manifest *m1, struct manifest *m2)
{
	GList *list1, *list2;
	struct file *file1, *file2;

	if (!m1) {
		printf("Maximizing manifests failed: No m1 manifest!\n");
		return;
	}

	LOG(NULL, "Maximizing", "%s to full", m1->component);

	if (!m2) {
		printf("Maximizing manifests failed: No m2 manifest!\n");
		return;
	}

	m1->files = g_list_sort(m1->files, file_sort_filename);
	m2->files = g_list_sort(m2->files, file_sort_filename);

	list1 = g_list_first(m1->files);
	list2 = g_list_first(m2->files);

	while (list1 && list2) {
		int ret;
		file1 = list1->data;
		file2 = list2->data;

		ret = strcmp(file1->filename, file2->filename);
		if (ret == 0) {
			if (!file1->is_deleted && file1->last_change > file2->last_change) {
				LOG(file1, "Update", "Moving %s to version %i", file1->filename, file1->last_change);
				file2->last_change = file1->last_change;
			}
			list1 = g_list_next(list1);
			list2 = g_list_next(list2);
			continue;
		}

		if (ret < 0) {
			list1 = g_list_next(list1);
			continue;
		}
		list2 = g_list_next(list2);
	}
}

/*
 * We need this in case files move modules;
 * the "full" list must be the maximum of all the versions of
 * all the modules for things to work out.
 */
void maximize_to_full(struct manifest *MoM, struct manifest *full)
{
	struct manifest *sub;
	GList *item;

	item = MoM->submanifests;
	while (item) {
		sub = item->data;
		item = g_list_next(item);
		maximize_version_manifests(sub, full);
	}
}

void recurse_manifest(struct manifest *manifest)
{
	GList *list;
	struct file *file;
	struct manifest *sub;

	list = manifest->manifests;
	while (list) {
		file = list->data;
		list = g_list_next(list);

		if (1) {
			int version2;
			version2 = file->last_change;

			sub = manifest_from_file(version2, file->filename);
			manifest->submanifests = g_list_prepend(manifest->submanifests, sub);
		}
	}
}

void consolidate_submanifests(struct manifest *manifest)
{
	GList *list, *next;
	struct manifest *sub;
	struct file *file1, *file2;

	/* Create a consolidated, sorted list of files from all of the
	 * manifests' lists of files.  */
	list = g_list_first(manifest->submanifests);
	while (list) {
		sub = list->data;
		list = g_list_next(list);
		if (!sub) {
			continue;
		}
		manifest->files = g_list_concat(sub->files, manifest->files);
		sub->files = NULL;
	}
	manifest->files = g_list_sort(manifest->files, file_sort_filename);

	/* Two pointers ("list" and "next") traverse the consolidated, filename sorted
	 * GList of files.  The "list" pointer is marched forward through the
	 * GList as long as it and the g_list_next(list) (aka "next") do not point
	 * to two objects with the same filename.  If the name is the same, then
	 * "list" and "next" point to the first and second in a series of perhaps
	 * many objects referring to the same filename.  As we determine which file out
	 * of multiples to keep in our consolidated, deduplicated, filename sorted list
	 * there are Manifest invariants to maintain.  The following table shows the
	 * associated decision matrix.  Note that "file" may be a file, directory or
	 * symlink.
	 *
	 *         | File 2:
	 *         |  A'    B'    C'    D'
	 * File 1: |------------------------
	 *    A    |  -  |  2  |  2  |  2  |
	 *    B    |  1  |  -  |  2  |  2  |
	 *    C    |  1  |  1  |  -  |  X  |
	 *    D    |  1  |  1  |  X  |  X  |
	 *
	 *   State for file1 {A,B,C,D}
	 *         for file2 {A',B',C',D'}
	 *       A:  is_deleted && !is_rename
	 *       B:  is_deleted &&  is_rename
	 *       C: !is_deleted && (file1->hash == file2->hash)
	 *       D: !is_deleted && (file1->hash != file2->hash)
	 *
	 *   Action
	 *       -: Don't Care   - choose/remove either file
	 *       X: Error State  - remove both files, LOG error
	 *       1: choose file1 - remove file2
	 *       2: choose file2 - remove file1
	 *
	 * NOTE: the code below could be rewritten to be more "efficient", but clarity
	 *       and concreteness here are of utmost importance if we are to correctly
	 *       maintain the installed system's state in the filesystem across updates
	 */
	list = g_list_first(manifest->files);
	while (list) {
		next = g_list_next(list);
		if (next == NULL) {
			break;
		}
		file1 = list->data;
		file2 = next->data;

		if (strcmp(file1->filename, file2->filename)) {
			list = next;
			continue;
		} /* from here on, file1 and file2 have a filename match */

		/* (case 1) A'                     : choose file1 */
		if (file2->is_deleted && !file2->is_rename) {
			manifest->files = g_list_delete_link(manifest->files, next);
			continue;
		}
		/* (case 2) A                      : choose file2 */
		if (file1->is_deleted && !file1->is_rename) {
			manifest->files = g_list_delete_link(manifest->files, list);
			list = next;
			continue;
		}
		/* (case 3) B' AND NOT A           : choose file 1*/
		if (file2->is_deleted && file2->is_rename) { /* && !(file1->is_deleted && !file1->is_rename) */
			manifest->files = g_list_delete_link(manifest->files, next);
			continue;
		}

		/* (case 4) B AND NOT (A' OR B')   : choose file2 */
		if (file1->is_deleted && file1->is_rename) { /* && !(file2->is_deleted) */
			manifest->files = g_list_delete_link(manifest->files, list);
			list = next;
			continue;
		}

		/* (case 5) C and C'               : choose file1 */
		if (!file1->is_deleted && !file2->is_deleted && hash_compare(file1->hash, file2->hash)) {
			manifest->files = g_list_delete_link(manifest->files, next);
			continue;
		}

		/* (case 6) all others constitute errors */
		LOG(NULL, "unhandled filename pair: file1", "%s %s (%d), file2 %s %s (%d)",
		    file1->filename, file1->hash, file1->last_change,
		    file1->filename, file2->hash, file2->last_change);
		manifest->files = g_list_delete_link(manifest->files, list);
		list = g_list_next(next);
		manifest->files = g_list_delete_link(manifest->files, next);
		printf("CONFLICT IN MANIFESTS\n");
	}
}

int previous_version_manifest(struct manifest *mom, char *name)
{
	GList *list;
	struct manifest *sub;

	list = g_list_first(mom->submanifests);
	while (list) {
		sub = list->data;
		list = g_list_next(list);
		if (!sub) {
			continue;
		}

		if (strcmp(sub->component, name) == 0) {
			return sub->prevversion;
		}
	}
	return 0;
}
