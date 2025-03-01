/* apk_fs.h - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2021 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef APK_FS_H
#define APK_FS_H

#include "apk_context.h"
#include "apk_io.h"
#include "apk_pathbuilder.h"

#define APK_FS_CTRL_COMMIT	1
#define APK_FS_CTRL_APKNEW	2
#define APK_FS_CTRL_CANCEL	3
#define APK_FS_CTRL_DELETE	4

#define APK_FS_DIR_MODIFIED	1

struct apk_fsdir_ops;

struct apk_fsdir {
	struct apk_ctx *ac;
	const struct apk_fsdir_ops *ops;
	struct apk_pathbuilder pb;
	apk_blob_t pkgctx;
};

struct apk_fsdir_ops {
	int (*dir_create)(struct apk_fsdir *, mode_t);
	int (*dir_delete)(struct apk_fsdir *);
	int (*dir_check)(struct apk_fsdir *, mode_t, uid_t, gid_t);
	int (*dir_update_perms)(struct apk_fsdir *, mode_t, uid_t, gid_t);

	int (*file_extract)(struct apk_ctx *, const struct apk_file_info *, struct apk_istream *, apk_progress_cb, void *, unsigned int, apk_blob_t);
	int (*file_control)(struct apk_fsdir *, apk_blob_t, int);
	int (*file_digest)(struct apk_fsdir *, apk_blob_t, uint8_t alg, struct apk_digest *);
};

#define APK_FSEXTRACTF_NO_CHOWN		0x0001
#define APK_FSEXTRACTF_NO_OVERWRITE	0x0002

int apk_fs_extract(struct apk_ctx *, const struct apk_file_info *, struct apk_istream *, apk_progress_cb, void *, unsigned int, apk_blob_t);

void apk_fsdir_get(struct apk_fsdir *, apk_blob_t dir, struct apk_ctx *, apk_blob_t);

static inline int apk_fsdir_create(struct apk_fsdir *fs, mode_t mode) {
	return fs->ops->dir_create(fs, mode);
}
static inline int apk_fsdir_delete(struct apk_fsdir *fs) {
	return fs->ops->dir_delete(fs);
}
static inline int apk_fsdir_check(struct apk_fsdir *fs, mode_t mode, uid_t uid, gid_t gid) {
	return fs->ops->dir_check(fs, mode, uid, gid);
}
static inline int apk_fsdir_update_perms(struct apk_fsdir *fs, mode_t mode, uid_t uid, gid_t gid) {
	return fs->ops->dir_update_perms(fs, mode, uid, gid);
}

static inline int apk_fsdir_file_control(struct apk_fsdir *fs, apk_blob_t filename, int ctrl) {
	return fs->ops->file_control(fs, filename, ctrl);
}
static inline int apk_fsdir_file_digest(struct apk_fsdir *fs, apk_blob_t filename, uint8_t alg, struct apk_digest *dgst) {
	return fs->ops->file_digest(fs, filename, alg, dgst);
}

#endif
