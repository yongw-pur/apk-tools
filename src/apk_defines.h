/* apk_defines.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef APK_DEFINES_H
#define APK_DEFINES_H

#include <endian.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))
#define BIT(x)		(1 << (x))
#define min(a, b)	((a) < (b) ? (a) : (b))
#define max(a, b)	((a) > (b) ? (a) : (b))

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0L
#endif

enum {
	APKE_EOF = 1024,
	APKE_DNS,
	APKE_URL_FORMAT,
	APKE_CRYPTO_ERROR,
	APKE_CRYPTO_NOT_SUPPORTED,
	APKE_CRYPTO_KEY_FORMAT,
	APKE_SIGNATURE_FAIL,
	APKE_SIGNATURE_UNTRUSTED,
	APKE_SIGNATURE_INVALID,
	APKE_FORMAT_NOT_SUPPORTED,
	APKE_ADB_COMPRESSION,
	APKE_ADB_HEADER,
	APKE_ADB_VERSION,
	APKE_ADB_SCHEMA,
	APKE_ADB_BLOCK,
	APKE_ADB_SIGNATURE,
	APKE_ADB_NO_FROMSTRING,
	APKE_ADB_LIMIT,
	APKE_ADB_DEPENDENCY_FORMAT,
	APKE_ADB_PACKAGE_FORMAT,
	APKE_V2DB_FORMAT,
	APKE_V2PKG_FORMAT,
	APKE_V2PKG_INTEGRITY,
	APKE_V2NDX_FORMAT,
	APKE_PACKAGE_NOT_FOUND,
	APKE_INDEX_STALE,
	APKE_FILE_INTEGRITY,
	APKE_CACHE_NOT_AVAILABLE,
	APKE_UVOL_NOT_AVAILABLE,
	APKE_UVOL_ERROR,
	APKE_UVOL_ROOT,
	APKE_REMOTE_IO,
};

static inline void *ERR_PTR(long error) { return (void*) error; }
static inline void *ERR_CAST(const void *ptr) { return (void*) ptr; }
static inline int PTR_ERR(const void *ptr) { return (int)(long) ptr; }
static inline int IS_ERR(const void *ptr) { return (unsigned long)ptr >= (unsigned long)-4095; }

#if defined __GNUC__ && __GNUC__ == 2 && __GNUC_MINOR__ < 96
#define __builtin_expect(x, expected_value) (x)
#endif

#ifndef likely
#define likely(x) __builtin_expect((!!(x)),1)
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect((!!(x)),0)
#endif

#ifndef typeof
#define typeof(x) __typeof__(x)
#endif

#ifndef container_of
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#define ROUND_DOWN(x,a)		((x) & ~(a-1))
#define ROUND_UP(x,a)		(((x)+(a)-1) & ~((a)-1))

/* default architecture for APK packages. */
#if defined(__x86_64__)
#define APK_DEFAULT_ARCH        "x86_64"
#elif defined(__i386__)
#define APK_DEFAULT_ARCH        "x86"
#elif defined(__powerpc__) && !defined(__powerpc64__)
#define APK_DEFAULT_ARCH	"ppc"
#elif defined(__powerpc64__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define APK_DEFAULT_ARCH	"ppc64"
#elif defined(__powerpc64__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define APK_DEFAULT_ARCH	"ppc64le"
#elif defined(__arm__) && defined(__ARM_PCS_VFP) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ && __ARM_ARCH>=7
#define APK_DEFAULT_ARCH	"armv7"
#elif defined(__arm__) && defined(__ARM_PCS_VFP) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define APK_DEFAULT_ARCH	"armhf"
#elif defined(__arm__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define APK_DEFAULT_ARCH	"armel"
#elif defined(__aarch64__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define APK_DEFAULT_ARCH	"aarch64"
#elif defined(__s390x__)
#define APK_DEFAULT_ARCH	"s390x"
#elif defined(__mips64) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define APK_DEFAULT_ARCH	"mips64"
#elif defined(__mips64) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define APK_DEFAULT_ARCH	"mips64el"
#elif defined(__mips__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define APK_DEFAULT_ARCH	"mips"
#elif defined(__mips__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define APK_DEFAULT_ARCH	"mipsel"
#elif defined(__riscv) && __riscv_xlen == 32
#define APK_DEFAULT_ARCH	"riscv32"
#elif defined(__riscv) && __riscv_xlen == 64
#define APK_DEFAULT_ARCH	"riscv64"
#elif defined(__loongarch__) && defined(__loongarch32) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define APK_DEFAULT_ARCH	"loongarch32"
#elif defined(__loongarch__) && defined(__loongarchx32) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define APK_DEFAULT_ARCH	"loongarchx32"
#elif defined(__loongarch__) && defined(__loongarch64) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define APK_DEFAULT_ARCH	"loongarch64"
#else
#error APK_DEFAULT_ARCH not detected for this architecture
#endif

#define APK_MAX_REPOS		32	/* see struct apk_package */
#define APK_MAX_TAGS		16	/* see solver; unsigned short */
#define APK_CACHE_CSUM_BYTES	4

static inline size_t apk_calc_installed_size(size_t size)
{
	const size_t bsize = 4 * 1024;

	return (size + bsize - 1) & ~(bsize - 1);
}
static inline size_t muldiv(size_t a, size_t b, size_t c)
{
	unsigned long long tmp;
	tmp = a;
	tmp *= b;
	tmp /= c;
	return (size_t) tmp;
}
static inline size_t mulmod(size_t a, size_t b, size_t c)
{
	unsigned long long tmp;
	tmp = a;
	tmp *= b;
	tmp %= c;
	return (size_t) tmp;
}

static inline uint32_t get_unaligned32(const void *ptr)
{
#if defined(__x86_64__) || defined(__i386__)
	return *(const uint32_t *)ptr;
#else
	const uint8_t *p = ptr;
	return p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24;
#endif
}

typedef void (*apk_progress_cb)(void *cb_ctx, size_t);

time_t apk_get_build_time(void);

void *apk_array_resize(void *array, size_t new_size, size_t elem_size);

#define APK_ARRAY(array_type_name, elem_type_name)			\
	struct array_type_name {					\
		size_t num;						\
		elem_type_name item[];					\
	};								\
	static inline void						\
	array_type_name##_init(struct array_type_name **a)		\
	{								\
		*a = apk_array_resize(NULL, 0, 0);			\
	}								\
	static inline void						\
	array_type_name##_free(struct array_type_name **a)		\
	{								\
		*a = apk_array_resize(*a, 0, 0);			\
	}								\
	static inline void						\
	array_type_name##_resize(struct array_type_name **a, size_t size)\
	{								\
		*a = apk_array_resize(*a, size, sizeof(elem_type_name));\
	}								\
	static inline void						\
	array_type_name##_copy(struct array_type_name **a, struct array_type_name *b)\
	{								\
		if (*a == b) return;					\
		*a = apk_array_resize(*a, b->num, sizeof(elem_type_name));\
		memcpy((*a)->item, b->item, b->num * sizeof(elem_type_name));\
	}								\
	static inline elem_type_name *					\
	array_type_name##_add(struct array_type_name **a)		\
	{								\
		int size = 1 + ((*a) ? (*a)->num : 0);			\
		*a = apk_array_resize(*a, size, sizeof(elem_type_name));\
		return &(*a)->item[size-1];				\
	}

APK_ARRAY(apk_string_array, char *);

#define foreach_array_item(iter, array) \
	for (iter = &(array)->item[0]; iter < &(array)->item[(array)->num]; iter++)

#define LIST_HEAD(name) struct list_head name = { &name, &name }
#define LIST_END (void *) 0xe01
#define LIST_POISON1 (void *) 0xdeadbeef
#define LIST_POISON2 (void *) 0xabbaabba

struct hlist_node {
	struct hlist_node *next;
};

struct hlist_head {
	struct hlist_node *first;
};

static inline int hlist_empty(const struct hlist_head *h)
{
	return !h->first;
}

static inline int hlist_hashed(const struct hlist_node *n)
{
	return n->next != NULL;
}

static inline void __hlist_del(struct hlist_node *n, struct hlist_node **pprev)
{
	*pprev = n->next;
	n->next = NULL;
}

static inline void hlist_del(struct hlist_node *n, struct hlist_head *h)
{
	struct hlist_node **pp = &h->first;

	while (*pp != NULL && *pp != LIST_END && *pp != n)
		pp = &(*pp)->next;

	if (*pp == n)
		__hlist_del(n, pp);
}

static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
	struct hlist_node *first = h->first;

	n->next = first ? first : LIST_END;
	h->first = n;
}

static inline void hlist_add_after(struct hlist_node *n, struct hlist_node **prev)
{
	n->next = *prev ? *prev : LIST_END;
	*prev = n;
}

static inline struct hlist_node **hlist_tail_ptr(struct hlist_head *h)
{
	struct hlist_node *n = h->first;
	if (n == NULL || n == LIST_END)
		return &h->first;
	while (n->next != NULL && n->next != LIST_END)
		n = n->next;
	return &n->next;
}

#define hlist_entry(ptr, type, member) container_of(ptr,type,member)

#define hlist_for_each(pos, head) \
	for (pos = (head)->first; pos && pos != LIST_END; \
	     pos = pos->next)

#define hlist_for_each_safe(pos, n, head) \
	for (pos = (head)->first; pos && pos != LIST_END && \
		({ n = pos->next; 1; }); \
	     pos = n)

#define hlist_for_each_entry(tpos, pos, head, member)			 \
	for (pos = (head)->first;					 \
	     pos && pos != LIST_END  &&					 \
		({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
	     pos = pos->next)

#define hlist_for_each_entry_safe(tpos, pos, n, head, member) 		 \
	for (pos = (head)->first;					 \
	     pos && pos != LIST_END && ({ n = pos->next; 1; }) && 	 \
		({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
	     pos = n)


struct list_head {
	struct list_head *next, *prev;
};

static inline void list_init(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}

static inline void list_del_init(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = NULL;
	entry->prev = NULL;
}

static inline int list_hashed(const struct list_head *n)
{
	return n->next != n && n->next != NULL;
}

static inline int list_empty(const struct list_head *n)
{
	return n->next == n;
}

static inline struct list_head *__list_pop(struct list_head *head)
{
	struct list_head *n = head->next;
	list_del_init(n);
	return n;
}

#define list_entry(ptr, type, member) container_of(ptr,type,member)

#define list_pop(head, type, member) container_of(__list_pop(head),type,member)

#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

#endif
