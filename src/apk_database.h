/* apk_database.h - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef APK_PKGDB_H
#define APK_PKGDB_H

#include "apk_version.h"
#include "apk_hash.h"
#include "apk_atom.h"
#include "apk_package.h"
#include "apk_io.h"
#include "apk_context.h"

#include "apk_provider_data.h"
#include "apk_solver_data.h"

struct apk_name;
APK_ARRAY(apk_name_array, struct apk_name *);

struct apk_db_acl {
	mode_t mode;
	uid_t uid;
	gid_t gid;
	struct apk_checksum xattr_csum;
};

struct apk_db_file {
	struct hlist_node hash_node;
	struct hlist_node diri_files_list;

	struct apk_db_dir_instance *diri;
	struct apk_db_acl *acl;

	unsigned short audited : 1;
	unsigned short namelen : 15;
	struct apk_checksum csum;
	char name[];
};

enum apk_protect_mode {
	APK_PROTECT_NONE = 0,
	APK_PROTECT_CHANGED,
	APK_PROTECT_SYMLINKS_ONLY,
	APK_PROTECT_ALL,
};

struct apk_protected_path {
	char *relative_pattern;
	unsigned protect_mode : 3;
};
APK_ARRAY(apk_protected_path_array, struct apk_protected_path);

struct apk_db_dir {
	apk_hash_node hash_node;
	unsigned long hash;

	struct apk_db_dir *parent;
	struct apk_protected_path_array *protected_paths;
	mode_t mode;
	uid_t uid;
	gid_t gid;

	unsigned short refs;
	unsigned short namelen;

	unsigned protect_mode : 3;
	unsigned has_protected_children : 1;

	unsigned seen : 1;
	unsigned created : 1;
	unsigned modified : 1;
	unsigned update_permissions : 1;

	char rooted_name[1];
	char name[];
};

#define DIR_FILE_FMT			"%s%s%s"
#define DIR_FILE_PRINTF(dir,file)	(dir)->name, (dir)->namelen ? "/" : "", (file)->name

struct apk_db_dir_instance {
	struct hlist_node pkg_dirs_list;
	struct hlist_head owned_files;
	struct apk_package *pkg;
	struct apk_db_dir *dir;
	struct apk_db_acl *acl;
};

struct apk_name {
	apk_hash_node hash_node;
	char *name;
	struct apk_provider_array *providers;
	struct apk_name_array *rdepends;
	struct apk_name_array *rinstall_if;
	unsigned is_dependency : 1;
	unsigned auto_select_virtual: 1;
	unsigned priority : 2;
	unsigned int foreach_genid;
	union {
		struct apk_solver_name_state ss;
		void *state_ptr;
		int state_int;
	};
};

struct apk_repository {
	const char *url;
	struct apk_checksum csum;
	apk_blob_t description;
};

#define APK_REPOSITORY_CACHED		0
#define APK_REPOSITORY_FIRST_CONFIGURED	1

#define APK_DEFAULT_REPOSITORY_TAG	0
#define APK_DEFAULT_PINNING_MASK	BIT(APK_DEFAULT_REPOSITORY_TAG)

struct apk_repository_tag {
	unsigned int allowed_repos;
	apk_blob_t tag, plain_name;
};

struct apk_database {
	struct apk_ctx *ctx;
	int root_fd, lock_fd, cache_fd;
	unsigned num_repos, num_repo_tags;
	const char *cache_dir;
	char *cache_remount_dir, *root_proc_dir;
	unsigned long cache_remount_flags;
	apk_blob_t *arch;
	unsigned int local_repos, available_repos;
	unsigned int repo_update_errors, repo_update_counter;
	unsigned int pending_triggers;
	unsigned int extract_flags;
	int performing_self_upgrade : 1;
	int permanent : 1;
	int autoupdate : 1;
	int write_arch : 1;
	int script_dirs_checked : 1;
	int open_complete : 1;
	int compat_newfeatures : 1;
	int compat_notinstallable : 1;

	struct apk_dependency_array *world;
	struct apk_id_cache *id_cache;
	struct apk_protected_path_array *protected_paths;
	struct apk_repository repos[APK_MAX_REPOS];
	struct apk_repository_tag repo_tags[APK_MAX_TAGS];
	struct apk_atom_pool atoms;

	struct {
		struct apk_hash names;
		struct apk_hash packages;
	} available;

	struct {
		struct list_head packages;
		struct list_head triggers;
		struct apk_hash dirs;
		struct apk_hash files;
		struct {
			unsigned files;
			unsigned dirs;
			unsigned packages;
			size_t bytes;
		} stats;
	} installed;
};

typedef union apk_database_or_void {
	struct apk_database *db;
	void *ptr;
} apk_database_t __attribute__ ((__transparent_union__));

struct apk_name *apk_db_get_name(struct apk_database *db, apk_blob_t name);
struct apk_name *apk_db_query_name(struct apk_database *db, apk_blob_t name);
int apk_db_get_tag_id(struct apk_database *db, apk_blob_t tag);

struct apk_db_dir *apk_db_dir_ref(struct apk_db_dir *dir);
void apk_db_dir_unref(struct apk_database *db, struct apk_db_dir *dir, int allow_rmdir);
struct apk_db_dir *apk_db_dir_get(struct apk_database *db, apk_blob_t name);
struct apk_db_dir *apk_db_dir_query(struct apk_database *db, apk_blob_t name);
struct apk_db_file *apk_db_file_query(struct apk_database *db,
				      apk_blob_t dir, apk_blob_t name);

void apk_db_init(struct apk_database *db);
int apk_db_open(struct apk_database *db, struct apk_ctx *ctx);
void apk_db_close(struct apk_database *db);
int apk_db_write_config(struct apk_database *db);
int apk_db_permanent(struct apk_database *db);
int apk_db_check_world(struct apk_database *db, struct apk_dependency_array *world);
int apk_db_fire_triggers(struct apk_database *db);
int apk_db_run_script(struct apk_database *db, char *fn, char **argv);
void apk_db_update_directory_permissions(struct apk_database *db);
static inline time_t apk_db_url_since(struct apk_database *db, time_t since) {
	return apk_ctx_since(db->ctx, since);
}

struct apk_package *apk_db_pkg_add(struct apk_database *db, struct apk_package *pkg);
struct apk_package *apk_db_get_pkg(struct apk_database *db, struct apk_checksum *csum);
struct apk_package *apk_db_get_file_owner(struct apk_database *db, apk_blob_t filename);

int apk_db_index_read(struct apk_database *db, struct apk_istream *is, int repo);
int apk_db_index_read_file(struct apk_database *db, const char *file, int repo);
int apk_db_index_write(struct apk_database *db, struct apk_ostream *os);

int apk_db_add_repository(apk_database_t db, apk_blob_t repository);
struct apk_repository *apk_db_select_repo(struct apk_database *db,
					  struct apk_package *pkg);

int apk_repo_format_cache_index(apk_blob_t to, struct apk_repository *repo);
int apk_repo_format_item(struct apk_database *db, struct apk_repository *repo, struct apk_package *pkg,
			 int *fd, char *buf, size_t len);

unsigned int apk_db_get_pinning_mask_repos(struct apk_database *db, unsigned short pinning_mask);

int apk_db_cache_active(struct apk_database *db);
int apk_cache_download(struct apk_database *db, struct apk_repository *repo,
		       struct apk_package *pkg, int autoupdate,
		       apk_progress_cb cb, void *cb_ctx);

typedef void (*apk_cache_item_cb)(struct apk_database *db,
				  int dirfd, const char *name,
				  struct apk_package *pkg);
int apk_db_cache_foreach_item(struct apk_database *db, apk_cache_item_cb cb);

int apk_db_install_pkg(struct apk_database *db,
		       struct apk_package *oldpkg,
		       struct apk_package *newpkg,
		       apk_progress_cb cb, void *cb_ctx);

void apk_name_foreach_matching(struct apk_database *db, struct apk_string_array *filter, unsigned int match,
			       void (*cb)(struct apk_database *db, const char *match, struct apk_name *name, void *ctx),
			       void *ctx);

static inline uint8_t apk_dbf_digest(struct apk_db_file *dbf)
{
	uint8_t alg;
	if (!dbf) return APK_DIGEST_NONE;
	alg = apk_digest_alg_by_len(dbf->csum.type);
	if (alg == APK_DIGEST_SHA1 && dbf->diri->pkg->ipkg->sha256_160)
		alg = APK_DIGEST_SHA256_160;
	return alg;
}

#endif
