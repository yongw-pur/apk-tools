/* apk.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>

#include <fetch.h>

#include "apk_defines.h"
#include "apk_database.h"
#include "apk_applet.h"
#include "apk_blob.h"
#include "apk_print.h"
#include "apk_io.h"

#ifdef TEST_MODE
static const char *test_installed_db = NULL;
static const char *test_world = NULL;
static struct apk_string_array *test_repos;
#endif

char **apk_argv;

#ifdef TEST_MODE
time_t time(time_t *tloc)
{
	const time_t val = 1559567666;
	if (tloc) *tloc = val;
	return val;
}
#endif

static void version(struct apk_out *out, const char *prefix)
{
	apk_out_fmt(out, prefix, "apk-tools " APK_VERSION ", compiled for " APK_DEFAULT_ARCH ".");
#ifdef TEST_MODE
	apk_out_fmt(out, prefix, "TEST MODE BUILD. NOT FOR PRODUCTION USE.");
#endif
}

#define GLOBAL_OPTIONS(OPT) \
	OPT(OPT_GLOBAL_allow_untrusted,		"allow-untrusted") \
	OPT(OPT_GLOBAL_arch,			APK_OPT_ARG "arch") \
	OPT(OPT_GLOBAL_cache_dir,		APK_OPT_ARG "cache-dir") \
	OPT(OPT_GLOBAL_cache_max_age,		APK_OPT_ARG "cache-max-age") \
	OPT(OPT_GLOBAL_force,			APK_OPT_SH("f") "force") \
	OPT(OPT_GLOBAL_force_binary_stdout,	"force-binary-stdout") \
	OPT(OPT_GLOBAL_force_broken_world,	"force-broken-world") \
	OPT(OPT_GLOBAL_force_no_chroot,		"force-no-chroot") \
	OPT(OPT_GLOBAL_force_non_repository,	"force-non-repository") \
	OPT(OPT_GLOBAL_force_old_apk,		"force-old-apk") \
	OPT(OPT_GLOBAL_force_overwrite,		"force-overwrite") \
	OPT(OPT_GLOBAL_force_refresh,		"force-refresh") \
	OPT(OPT_GLOBAL_help,			APK_OPT_SH("h") "help") \
	OPT(OPT_GLOBAL_interactive,		APK_OPT_SH("i") "interactive") \
	OPT(OPT_GLOBAL_keys_dir,		APK_OPT_ARG "keys-dir") \
	OPT(OPT_GLOBAL_no_cache,		"no-cache") \
	OPT(OPT_GLOBAL_no_logfile,		"no-logfile") \
	OPT(OPT_GLOBAL_no_network,		"no-network") \
	OPT(OPT_GLOBAL_no_progress,		"no-progress") \
	OPT(OPT_GLOBAL_preserve_env,		"preserve-env") \
	OPT(OPT_GLOBAL_print_arch,		"print-arch") \
	OPT(OPT_GLOBAL_progress,		"progress") \
	OPT(OPT_GLOBAL_progress_fd,		APK_OPT_ARG "progress-fd") \
	OPT(OPT_GLOBAL_purge,			"purge") \
	OPT(OPT_GLOBAL_quiet,			APK_OPT_SH("q") "quiet") \
	OPT(OPT_GLOBAL_repositories_file,	APK_OPT_ARG "repositories-file") \
	OPT(OPT_GLOBAL_repository,		APK_OPT_ARG APK_OPT_SH("X") "repository") \
	OPT(OPT_GLOBAL_root,			APK_OPT_ARG APK_OPT_SH("p") "root") \
	OPT(OPT_GLOBAL_update_cache,		APK_OPT_SH("U") "update-cache") \
	OPT(OPT_GLOBAL_verbose,			APK_OPT_SH("v") "verbose") \
	OPT(OPT_GLOBAL_version,			APK_OPT_SH("V") "version") \
	OPT(OPT_GLOBAL_wait,			APK_OPT_ARG "wait") \

#define TEST_OPTIONS(OPT) \
	OPT(OPT_GLOBAL_test_instdb,		APK_OPT_ARG "test-instdb") \
	OPT(OPT_GLOBAL_test_repo,		APK_OPT_ARG "test-repo") \
	OPT(OPT_GLOBAL_test_world,		APK_OPT_ARG "test-world")


#ifdef TEST_MODE
APK_OPT_GROUP2(optiondesc_global, "Global", GLOBAL_OPTIONS, TEST_OPTIONS);
#else
APK_OPT_GROUP(optiondesc_global, "Global", GLOBAL_OPTIONS);
#endif

static int option_parse_global(void *ctx, struct apk_ctx *ac, int opt, const char *optarg)
{
	struct apk_out *out = &ac->out;
	switch (opt) {
	case OPT_GLOBAL_help:
		return -EINVAL;
	case OPT_GLOBAL_root:
		ac->root = optarg;
		break;
	case OPT_GLOBAL_keys_dir:
		ac->keys_dir = optarg;
		break;
	case OPT_GLOBAL_repositories_file:
		ac->repositories_file = optarg;
		break;
	case OPT_GLOBAL_repository:
		*apk_string_array_add(&ac->repository_list) = (char*) optarg;
		break;
	case OPT_GLOBAL_quiet:
		if (ac->out.verbosity) ac->out.verbosity--;
		break;
	case OPT_GLOBAL_verbose:
		ac->out.verbosity++;
		break;
	case OPT_GLOBAL_version:
		version(out, NULL);
		return -ESHUTDOWN;
	case OPT_GLOBAL_force:
		ac->force |= APK_FORCE_OVERWRITE | APK_FORCE_OLD_APK
			| APK_FORCE_BROKEN_WORLD | APK_FORCE_NON_REPOSITORY
			| APK_FORCE_BINARY_STDOUT;
		break;
	case OPT_GLOBAL_force_overwrite:
		ac->force |= APK_FORCE_OVERWRITE;
		break;
	case OPT_GLOBAL_force_old_apk:
		ac->force |= APK_FORCE_OLD_APK;
		break;
	case OPT_GLOBAL_force_broken_world:
		ac->force |= APK_FORCE_BROKEN_WORLD;
		break;
	case OPT_GLOBAL_force_refresh:
		ac->force |= APK_FORCE_REFRESH;
		break;
	case OPT_GLOBAL_force_no_chroot:
		ac->flags |= APK_NO_CHROOT;
		break;
	case OPT_GLOBAL_force_non_repository:
		ac->force |= APK_FORCE_NON_REPOSITORY;
		break;
	case OPT_GLOBAL_force_binary_stdout:
		ac->force |= APK_FORCE_BINARY_STDOUT;
		break;
	case OPT_GLOBAL_interactive:
		ac->flags |= APK_INTERACTIVE;
		break;
	case OPT_GLOBAL_preserve_env:
		ac->flags |= APK_PRESERVE_ENV;
		break;
	case OPT_GLOBAL_progress:
		ac->progress.out = &ac->out;
		break;
	case OPT_GLOBAL_no_progress:
		ac->progress.out = NULL;
		break;
	case OPT_GLOBAL_progress_fd:
		ac->progress.fd = atoi(optarg);
		break;
	case OPT_GLOBAL_allow_untrusted:
		ac->flags |= APK_ALLOW_UNTRUSTED;
		break;
	case OPT_GLOBAL_purge:
		ac->flags |= APK_PURGE;
		break;
	case OPT_GLOBAL_wait:
		ac->lock_wait = atoi(optarg);
		break;
	case OPT_GLOBAL_no_logfile:
		ac->flags |= APK_NO_LOGFILE;
		break;
	case OPT_GLOBAL_no_network:
		ac->flags |= APK_NO_NETWORK;
		break;
	case OPT_GLOBAL_no_cache:
		ac->flags |= APK_NO_CACHE;
		break;
	case OPT_GLOBAL_cache_dir:
		ac->cache_dir = optarg;
		break;
	case OPT_GLOBAL_update_cache:
		/* Make it one minute, to avoid updating indexes twice
		 * when doing self-upgrade's re-exec */
		ac->cache_max_age = 60;
		break;
	case OPT_GLOBAL_cache_max_age:
		ac->cache_max_age = atoi(optarg) * 60;
		break;
	case OPT_GLOBAL_arch:
		ac->arch = optarg;
		break;
	case OPT_GLOBAL_print_arch:
		puts(APK_DEFAULT_ARCH);
		return -ESHUTDOWN;
#ifdef TEST_MODE
	case OPT_GLOBAL_test_repo:
		*apk_string_array_add(&test_repos) = (char*) optarg;
		break;
	case OPT_GLOBAL_test_instdb:
		test_installed_db = optarg;
		break;
	case OPT_GLOBAL_test_world:
		test_world = optarg;
		break;
#endif
	default:
		return -ENOTSUP;
	}
	return 0;
}

const struct apk_option_group optgroup_global = {
	.desc = optiondesc_global,
	.parse = option_parse_global,
};

#define COMMIT_OPTIONS(OPT) \
	OPT(OPT_COMMIT_clean_protected,		"clean-protected") \
	OPT(OPT_COMMIT_initramfs_diskless_boot,	"initramfs-diskless-boot") \
	OPT(OPT_COMMIT_no_commit_hooks,		"no-commit-hooks") \
	OPT(OPT_COMMIT_no_scripts,		"no-scripts") \
	OPT(OPT_COMMIT_overlay_from_stdin,	"overlay-from-stdin") \
	OPT(OPT_COMMIT_simulate,		APK_OPT_SH("s") "simulate")

APK_OPT_GROUP(optiondesc_commit, "Commit", COMMIT_OPTIONS);

static int option_parse_commit(void *ctx, struct apk_ctx *ac, int opt, const char *optarg)
{
	switch (opt) {
	case OPT_COMMIT_simulate:
		ac->flags |= APK_SIMULATE;
		break;
	case OPT_COMMIT_clean_protected:
		ac->flags |= APK_CLEAN_PROTECTED;
		break;
	case OPT_COMMIT_overlay_from_stdin:
		ac->flags |= APK_OVERLAY_FROM_STDIN;
		break;
	case OPT_COMMIT_no_scripts:
		ac->flags |= APK_NO_SCRIPTS;
		break;
	case OPT_COMMIT_no_commit_hooks:
		ac->flags |= APK_NO_COMMIT_HOOKS;
		break;
	case OPT_COMMIT_initramfs_diskless_boot:
		ac->open_flags |= APK_OPENF_CREATE;
		ac->flags |= APK_NO_COMMIT_HOOKS;
		ac->force |= APK_FORCE_OVERWRITE | APK_FORCE_OLD_APK
			|  APK_FORCE_BROKEN_WORLD | APK_FORCE_NON_REPOSITORY;
		break;
	default:
		return -ENOTSUP;
	}
	return 0;
}

const struct apk_option_group optgroup_commit = {
	.desc = optiondesc_commit,
	.parse = option_parse_commit,
};

static int usage(struct apk_out *out, struct apk_applet *applet)
{
	version(out, NULL);
	apk_applet_help(applet, out);
	return 1;
}

static struct apk_applet *deduce_applet(int argc, char **argv)
{
	struct apk_applet *a;
	const char *prog;
	int i;

	prog = strrchr(argv[0], '/');
	if (prog == NULL)
		prog = argv[0];
	else
		prog++;

	if (strncmp(prog, "apk_", 4) == 0)
		return apk_applet_find(prog + 4);

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') continue;
		a = apk_applet_find(argv[i]);
		if (a) return a;
	}

	return NULL;
}

static int parse_options(int argc, char **argv, struct apk_applet *applet, void *ctx, struct apk_ctx *ac)
{
	struct apk_out *out = &ac->out;
	const struct apk_option_group *default_optgroups[] = { &optgroup_global, NULL };
	const struct apk_option_group *og, **optgroups = default_optgroups;
	struct option all_options[80], *opt;
	char short_options[256], *sopt;
	unsigned short short_option_val[64];
	int r, p, help_requested = 0, num_short;

	memset(short_option_val, 0, sizeof short_option_val);

	if (applet && applet->optgroups[0]) optgroups = applet->optgroups;

	for (p = 0, opt = &all_options[0], sopt = short_options; (og = optgroups[p]) != 0; p++) {
		assert(opt < &all_options[ARRAY_SIZE(all_options)]);
		assert(sopt < &short_options[sizeof short_options]);
		const char *d = og->desc + strlen(og->desc) + 1;
		for (r = 0; *d; r++) {
			opt->val = (p << 10) + r;
			opt->flag = 0;
			opt->has_arg = no_argument;
			if ((unsigned char)*d == 0xaf) {
				opt->has_arg = required_argument;
				d++;
			}
			num_short = 0;
			if ((unsigned char)*d >= 0xf0)
				num_short = *d++ & 0x0f;
			for (; num_short > 0; num_short--) {
				unsigned char ch = *(unsigned char *)d;
				assert(ch >= 64 && ch < 128);
				short_option_val[ch-64] = opt->val;
				*sopt++ = *d++;
				if (opt->has_arg != no_argument)
					*sopt++ = ':';
			}
			opt->name = d;
			opt++;
			d += strlen(d) + 1;
		}
	}
	opt->name = 0;
	*sopt = 0;

	r = 0;
	while ((p = getopt_long(argc, argv, short_options, all_options, NULL)) != -1) {
		if (p >= 64 && p < 128) p = short_option_val[p - 64];
		og = optgroups[p >> 10];
		r = og->parse(ctx, ac, p & 0x3ff, optarg);
		if (r == 0) continue;
		if (r == -EINVAL) {
			help_requested = 1;
			continue;
		}
		if (r != -ENOTSUP) return r;
	}

	if (help_requested || r == -ENOTSUP)
		return usage(out, applet);

	return 0;
}

static void setup_automatic_flags(struct apk_ctx *ac)
{
	const char *tmp;

	if ((tmp = getenv("APK_PROGRESS_CHAR")) != NULL)
		ac->progress.progress_char = tmp;
	else if ((tmp = getenv("LANG")) != NULL && strstr(tmp, "UTF-8") != NULL)
		ac->progress.progress_char = "\u2588";
	else
		ac->progress.progress_char = "#";

	if (!isatty(STDOUT_FILENO) || !isatty(STDERR_FILENO) ||
	    !isatty(STDIN_FILENO))
		return;

	/* Enable progress bar by default, except on dumb terminals. */
	if (!(tmp = getenv("TERM")) || strcmp(tmp, "dumb") != 0)
		ac->progress.out = &ac->out;

	if (!(ac->flags & APK_SIMULATE) &&
	    access("/etc/apk/interactive", F_OK) == 0)
		ac->flags |= APK_INTERACTIVE;
}

static struct apk_ctx ctx;
static struct apk_database db;

static void on_sigint(int s)
{
	apk_db_close(&db);
	exit(128 + s);
}

static void on_sigwinch(int s)
{
	apk_out_reset(&ctx.out);
}

static void setup_terminal(void)
{
	static char buf[200];
	setvbuf(stderr, buf, _IOLBF, sizeof buf);
	signal(SIGWINCH, on_sigwinch);
	signal(SIGPIPE, SIG_IGN);
}

int main(int argc, char **argv)
{
	void *applet_ctx = NULL;
	struct apk_out *out = &ctx.out;
	struct apk_string_array *args;
	struct apk_applet *applet;
	int r;

	apk_string_array_init(&args);
#ifdef TEST_MODE
	apk_string_array_init(&test_repos);
#endif

	apk_argv = malloc(sizeof(char*[argc+2]));
	memcpy(apk_argv, argv, sizeof(char*[argc]));
	apk_argv[argc] = NULL;
	apk_argv[argc+1] = NULL;

	apk_ctx_init(&ctx);
	umask(0);
	setup_terminal();

	applet = deduce_applet(argc, argv);
	if (applet != NULL) {
		if (applet->context_size != 0)
			applet_ctx = calloc(1, applet->context_size);
		ctx.open_flags = applet->open_flags;
		ctx.force |= applet->forced_force;
	}

	apk_crypto_init();
	setup_automatic_flags(&ctx);
	fetchConnectionCacheInit(32, 4);

	r = parse_options(argc, argv, applet, applet_ctx, &ctx);
	if (r != 0) goto err;

	if (applet == NULL) {
		if (argc > 1) {
			apk_err(out, "'%s' is not an apk command. See 'apk --help'.", argv[1]);
			return 1;
		}
		return usage(out, NULL);
	}

	argc -= optind;
	argv += optind;
	if (argc >= 1 && strcmp(argv[0], applet->name) == 0) {
		argc--;
		argv++;
	}

	apk_db_init(&db);
	signal(SIGINT, on_sigint);

#ifdef TEST_MODE
	ctx.open_flags &= ~(APK_OPENF_WRITE | APK_OPENF_CACHE_WRITE | APK_OPENF_CREATE);
	ctx.open_flags |= APK_OPENF_READ | APK_OPENF_NO_STATE | APK_OPENF_NO_REPOS;
	ctx.flags |= APK_SIMULATE;
	ctx.flags &= ~APK_INTERACTIVE;
#endif

	r = apk_ctx_prepare(&ctx);
	if (r != 0) goto err;

	apk_out_log_argv(&ctx.out, apk_argv);
	version(&ctx.out, APK_OUT_LOG_ONLY);

	if (ctx.open_flags) {
		r = apk_db_open(&db, &ctx);
		if (r != 0) {
			apk_err(out, "Failed to open apk database: %s", apk_error_str(r));
			goto err;
		}
	}

#ifdef TEST_MODE
	if (test_world != NULL) {
		apk_blob_t b = APK_BLOB_STR(test_world);
		apk_blob_pull_deps(&b, &db, &db.world);
	}
	if (test_installed_db != NULL) {
		apk_db_index_read(&db, apk_istream_from_file(AT_FDCWD, test_installed_db), -1);
	}
	for (int i = 0; i < test_repos->num; i++) {
		apk_blob_t spec = APK_BLOB_STR(test_repos->item[i]), name, tag;
		int repo_tag = 0, repo = APK_REPOSITORY_FIRST_CONFIGURED + i;

		if (spec.ptr[0] == '!') {
			/* cache's installed repository */
			spec.ptr++;
			spec.len--;
			repo = -2;
		}

		if (apk_blob_split(spec, APK_BLOB_STR(":"), &tag, &name)) {
			repo_tag = apk_db_get_tag_id(&db, tag);
		} else {
			name = spec;
		}

		r = apk_db_index_read(&db, apk_istream_from_file(AT_FDCWD, name.ptr), repo);
		if (r != 0) {
			apk_err(out, "Failed to open repository " BLOB_FMT " : %s", BLOB_PRINTF(name), apk_error_str(r));
			goto err;
		}

		if (repo != -2) {
			if (!(ctx.flags & APK_NO_NETWORK))
				db.available_repos |= BIT(repo);
			db.repo_tags[repo_tag].allowed_repos |= BIT(repo);
		}
	}
#endif

	apk_string_array_resize(&args, argc);
	memcpy(args->item, argv, argc * sizeof(*argv));

	r = applet->main(applet_ctx, &ctx, args);
	apk_db_close(&db);

#ifdef TEST_MODE
	/* in test mode, we need to always exit 0 since xargs dies otherwise */
	r = 0;
#endif

err:
	if (r == -ESHUTDOWN) r = 0;
	if (applet_ctx) free(applet_ctx);

	fetchConnectionCacheClose();
	apk_ctx_free(&ctx);
	apk_string_array_free(&args);
	free(apk_argv);

	if (r < 0) r = 250;
	if (r > 99) r = 99;
	return r;
}
