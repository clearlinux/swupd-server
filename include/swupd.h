#ifndef __INCLUDE_GUARD_SWUPD_H
#define __INCLUDE_GUARD_SWUPD_H

#include <glib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "config.h"

#define __unused__ __attribute__((__unused__))

// SWUPD_NUM_PACKS is also "PREV_CHECK" in releas tool swupd_bb.py (change both)
#define SWUPD_NUM_PACKS 4
#define SWUPD_NUM_MANIFEST_DELTAS 25

#define SWUPD_SERVER_STATE_DIR "/var/lib/update"

#if SWUPD_WITH_BSDTAR
#define TAR_COMMAND "bsdtar"
#define TAR_XATTR_ARGS ""
#define TAR_XATTR_ARGS_STRLIST
#define TAR_WARN_ARGS_STRLIST
#else
#define TAR_COMMAND "tar"
#define TAR_XATTR_ARGS "--xattrs --xattrs-include='*'"
#define TAR_XATTR_ARGS_STRLIST "--xattrs", "--xattrs-include='*'",
#define TAR_WARN_ARGS_STRLIST "--warning=no-timestamp",
#endif

#if SWUPD_WITH_SELINUX
#define TAR_PERM_ATTR_ARGS "--preserve-permissions --selinux " TAR_XATTR_ARGS
#define TAR_PERM_ATTR_ARGS_STRLIST TAR_XATTR_ARGS_STRLIST "--preserve-permissions", "--selinux"
#else
#define TAR_PERM_ATTR_ARGS "--preserve-permissions " TAR_XATTR_ARGS
#define TAR_PERM_ATTR_ARGS_STRLIST TAR_XATTR_ARGS_STRLIST "--preserve-permissions"
#endif

#define SWUPD_HASH_DIRNAME "DIRECTORY"

#if SWUPD_WITH_STATELESS
#define OS_IS_STATELESS 1
#else
#define OS_IS_STATELESS 0
#endif

/* Build types */
#define REGULAR_BUILD 0
#define FIX_BUILD 1
#define DEV_BUILD 2

/* We don't have BZIP2 support in Android.  This define translates to "ifdef 0"
 * and demarcates codes which otherwise would enable BZIP2.  If this define
 * is changed, the Android update client will break.  DO NOT CHANGE THIS
 * DEFINE IN AN ANDROID TARGETED BUILD WITHOUT ENABLING/TESTING BZIP2 SUPPORT
 * IN THE ANDROID CLIENT OS BUILD.
 */
#ifdef SWUPD_WITH_BZIP2
#include <bzlib.h>
#endif

/* Build toggle for lzma support.
 */
#ifdef SWUPD_WITH_LZMA
#include <lzma.h>
#endif

struct manifest {
	unsigned long long int format;
	int version;
	int prevversion;
	char *component;
	int count;
	uint64_t contentsize;
	GList *files;
	GList *manifests; /* as struct file */

	GList *submanifests; /* as struct manifest */

	GList *includes; /* struct manifests for all bundles included into this one */
};

struct file;
struct header;

struct update_stat {
	uint64_t st_mode;
	uint64_t st_uid;
	uint64_t st_gid;
	uint64_t st_rdev;
	uint64_t st_size;
};

#define DIGEST_LEN_SHA256 64
/* +1 for null termination */
#define SWUPD_HASH_LEN (DIGEST_LEN_SHA256 + 1)

struct file {
	char *filename;
	char hash[SWUPD_HASH_LEN];
	bool use_xattrs;
	int last_change;

	struct update_stat stat;

	/* the following are exclusive */
	unsigned int is_dir : 1;
	unsigned int is_file : 1;
	unsigned int is_link : 1;
	unsigned int is_deleted : 1;
	unsigned int is_manifest : 1;

	/* and these are modifiers */
	unsigned int is_config : 1;
	unsigned int is_state : 1;
	unsigned int is_boot : 1;
	unsigned int is_rename : 1;

	struct file *peer; /* same file in another manifest */

	/* data fields to help rename detection */
	double rename_score;
	struct file *rename_peer;
	char *alpha_only_filename; /* filename minus all numerics/etc */
	char *filetype;
	char *basename;
	char *dirname;
	/* end of rename detection fields */

	unsigned int multithread : 1; /* if set to 1, current file is computed in a
						multithreaded process for fullfile creation */
};

struct packdata {
	char *module;
	int from;
	int to;
	int fullcount;
	struct manifest *end_manifest;
};

extern int current_version;
extern int newversion;
extern int minversion;
extern unsigned long long int format;

extern char *state_dir;
extern char *packstage_dir;
extern char *image_dir;
extern char *staging_dir;

extern bool init_globals(void);
extern void free_globals(void);
extern bool set_format(char *);
extern void check_root(void);
extern bool set_state_dir(char *);
extern bool init_state_globals(void);
extern void free_state_globals(void);

extern int file_sort_hash(gconstpointer a, gconstpointer b);
extern int file_sort_filename(gconstpointer a, gconstpointer b);
extern int file_sort_version(gconstpointer a, gconstpointer b);

extern bool read_configuration_file(char *filename);
extern void release_configuration_data(void);
extern char *config_image_base(void);
extern char *config_output_dir(void);
extern char *config_empty_dir(void);
extern int config_initial_version(void);

extern void read_current_version(char *filename);
extern void write_new_version(char *filename, int version);
extern void write_cookiecrumbs_to_download_area(int version);
extern void ensure_version_image_exists(int version);
extern GList *get_last_versions_list(int next_version, int max_versions);

extern char *file_type_to_string(struct file *file);
extern struct manifest *manifest_from_file(int version, char *module);
extern void free_manifest(struct manifest *manifest);
extern struct manifest *alloc_manifest(int version, char *module);
extern int match_manifests(struct manifest *m1, struct manifest *m2);
extern void sort_manifest_by_version(struct manifest *manifest);
extern bool manifest_includes(struct manifest *manifest, char *component);
extern bool changed_includes(struct manifest *old, struct manifest *new);
extern int prune_manifest(struct manifest *manifest);
extern int remove_old_deleted_files(struct manifest *m1, struct manifest *m2);
extern void create_manifest_delta(int oldversion, int newversion, char *module);
extern void create_manifest_deltas(struct manifest *manifest, GList *last_versions_list);
extern void subtract_manifests_frontend(struct manifest *m1, struct manifest *m2);
extern void nest_manifest(struct manifest *parent, struct manifest *sub);
extern void nest_manifest_file(struct manifest *parent, struct file *file);
extern int manifest_subversion(struct manifest *parent, char *group);

extern void account_new_file(void);
extern void account_deleted_file(void);
extern void account_changed_file(void);
extern void print_statistics(int version1, int version2);

extern struct manifest *full_manifest_from_directory(int version);
extern struct manifest *sub_manifest_from_directory(char *component, int version);
extern void add_component_hashes_to_manifest(struct manifest *compm, struct manifest *fullm);
extern int write_manifest(struct manifest *manifest);

extern void hash_assign(char *src, char *dest);
extern bool hash_compare(char *hash1, char *hash2);
extern void hash_set_zeros(char *hash);
extern bool hash_is_zeros(char *hash);
extern int compute_hash(struct file *file, char *filename) __attribute__((warn_unused_result));

extern void prepare_delta_dir(struct manifest *manifest);
extern void create_fullfiles(struct manifest *manifest);
extern bool create_download_content_for_group(const char *group);
extern bool compute_hash_with_xattrs(const char *group);

extern void clean_package_caches(char *subdir);
extern void update_chroot(char *dir);
extern void chroot_create_full(int newversion);

extern void read_group_file(char *filename);
extern void release_group_file(void);
extern char *group_groups(char *group);
extern char *group_packages(char *group);
extern char *group_status(char *group);
extern char *next_group(void);

extern void apply_heuristics(struct manifest *manifest);
extern int make_pack(struct packdata *pack);

extern void maximize_to_full(struct manifest *MoM, struct manifest *full);
extern void recurse_manifest(struct manifest *manifest);
extern void consolidate_submanifests(struct manifest *manifest);
extern void populate_file_struct(struct file *file, char *filename);
extern void download_exta_base_content(void);

extern char *get_elapsed_time(struct timeval *t1, struct timeval *t2);
extern void init_log(const char *prefix, const char *bundle, int start, int end);
extern void init_log_stdout(void);
extern void close_log(int version, int exit_status);
extern void __log_message(struct file *file, char *msg, char *filename, int linenr, const char *fmt, ...);
#define LOG(file, msg, fmt...) __log_message(file, msg, __FILE__, __LINE__, fmt)
extern int previous_version_manifest(struct manifest *mom, char *name);

extern void type_change_detection(struct manifest *manifest);

extern void rename_detection(struct manifest *manifest);
extern void link_renames(GList *newfiles, int to_version);
extern void __create_delta(struct file *file, int from_version, char *from_hash);

extern void account_delta_hit(void);
extern void account_delta_miss(void);

extern FILE *fopen_exclusive(const char *filename); /* no mode, opens for write only */
extern void dump_file_info(struct file *file);
extern void string_or_die(char **strp, const char *fmt, ...);
extern void print_elapsed_time(const char *step, struct timeval *previous_time, struct timeval *current_time);
extern int system_argv(char *const argv[]);
extern int system_argv_fd(char *const argv[], int newstdin, int newstdout, int newstderr);
extern int system_argv_pipe(char *const argvp1[], int stdinp1, int stderrp1,
			    char *const argvp2[], int stdoutp2, int stderrp2);
extern int num_threads(float scaling);

#endif
