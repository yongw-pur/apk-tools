/* blob.c - Alpine Package Keeper (APK)
 *
 * Copyright (C) 2005-2008 Natanael Copa <n@tanael.org>
 * Copyright (C) 2008-2011 Timo Teräs <timo.teras@iki.fi>
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "apk_blob.h"
#include "apk_hash.h"

char *apk_blob_cstr(apk_blob_t blob)
{
	char *cstr;

	if (blob.len == 0)
		return strdup("");

	if (blob.ptr[blob.len-1] == 0)
		return strdup(blob.ptr);

	cstr = malloc(blob.len + 1);
	memcpy(cstr, blob.ptr, blob.len);
	cstr[blob.len] = 0;

	return cstr;
}

apk_blob_t apk_blob_dup(apk_blob_t blob)
{
	char *ptr = malloc(blob.len);
	if (!ptr) return APK_BLOB_NULL;
	memcpy(ptr, blob.ptr, blob.len);
	return APK_BLOB_PTR_LEN(ptr, blob.len);
}


#if defined(__i386__)
static unsigned long inline memspn(
		const unsigned char *ptr,
		unsigned long len,
		const unsigned char *array)
{
	unsigned long p = len;

	__asm__("cld ; xorl %%eax, %%eax\n"
		"1:\n"
		"lodsb\n"
		"btl %%eax, %2\n"
		"jnc 2f\n"
		"decl %1\n"
		"jnz 1b\n"
		"2:\n"
		: "+&S" (ptr), "+&r" (p)
		: "m" (*array)
		: "cc", "%eax");

	return p;
}

static unsigned long inline memcspn(
		const unsigned char *ptr,
		unsigned long len,
		const unsigned char *array)
{
	unsigned long p = len;

	__asm__("cld ; xorl %%eax, %%eax\n"
		"1:\n"
		"lodsb\n"
		"btl %%eax, %2\n"
		"jc 2f\n"
		"decl %1\n"
		"jnz 1b\n"
		"2:\n"
		: "+&S" (ptr), "+&r" (p)
		: "m" (*array)
		: "cc", "%eax");

	return p;
}

int apk_blob_spn(apk_blob_t blob, const apk_spn_match accept, apk_blob_t *l, apk_blob_t *r)
{
	unsigned int i;

	if (blob.len == 0)
		return 0;
	i = blob.len - memspn((unsigned char*) blob.ptr, blob.len, accept);
	if (i == blob.len)
		return 0;
	if (l != NULL)
		*l = APK_BLOB_PTR_LEN(blob.ptr, i);
	if (r != NULL)
		*r = APK_BLOB_PTR_LEN(blob.ptr+i, blob.len-i);
	return 1;
}

int apk_blob_cspn(apk_blob_t blob, const apk_spn_match reject, apk_blob_t *l, apk_blob_t *r)
{
	unsigned int i;

	if (blob.len == 0)
		return 0;
	i = blob.len - memcspn((unsigned char*) blob.ptr, blob.len, reject);
	if (i == blob.len)
		return 0;
	if (l != NULL)
		*l = APK_BLOB_PTR_LEN(blob.ptr, i);
	if (r != NULL)
		*r = APK_BLOB_PTR_LEN(blob.ptr+i, blob.len-i);
	return 1;
}
#else
static int inline test_bit(const unsigned char *array, unsigned char bit)
{
	return array[bit >> 3] & (1 << (bit & 7));
}

int apk_blob_spn(apk_blob_t blob, const apk_spn_match accept, apk_blob_t *l, apk_blob_t *r)
{
	int i;

	for (i = 0; i < blob.len; i++) {
		if (!test_bit(accept, blob.ptr[i])) {
			if (l != NULL)
				*l = APK_BLOB_PTR_LEN(blob.ptr, i);
			if (r != NULL)
				*r = APK_BLOB_PTR_LEN(blob.ptr+i, blob.len-i);
			return 1;
		}
	}
	return 0;
}

int apk_blob_cspn(apk_blob_t blob, const apk_spn_match reject, apk_blob_t *l, apk_blob_t *r)
{
	int i;

	for (i = 0; i < blob.len; i++) {
		if (test_bit(reject, blob.ptr[i])) {
			if (l != NULL)
				*l = APK_BLOB_PTR_LEN(blob.ptr, i);
			if (r != NULL)
				*r = APK_BLOB_PTR_LEN(blob.ptr+i, blob.len-i);
			return 1;
		}
	}
	return 0;
}
#endif

int apk_blob_rsplit(apk_blob_t blob, char split, apk_blob_t *l, apk_blob_t *r)
{
	char *sep;

	sep = memrchr(blob.ptr, split, blob.len);
	if (sep == NULL)
		return 0;

	if (l != NULL)
		*l = APK_BLOB_PTR_PTR(blob.ptr, sep - 1);
	if (r != NULL)
		*r = APK_BLOB_PTR_PTR(sep + 1, blob.ptr + blob.len - 1);

	return 1;
}

int apk_blob_split(apk_blob_t blob, apk_blob_t split, apk_blob_t *l, apk_blob_t *r)
{
	char *pos = blob.ptr, *end = blob.ptr + blob.len - split.len + 1;

	if (!pos || end < pos) return 0;

	while (1) {
		pos = memchr(pos, split.ptr[0], end - pos);
		if (!pos) return 0;

		if (split.len > 1 && memcmp(pos, split.ptr, split.len) != 0) {
			pos++;
			continue;
		}

		*l = APK_BLOB_PTR_PTR(blob.ptr, pos-1);
		*r = APK_BLOB_PTR_PTR(pos+split.len, blob.ptr+blob.len-1);
		return 1;
	}
}

apk_blob_t apk_blob_pushed(apk_blob_t buffer, apk_blob_t left)
{
	if (buffer.ptr + buffer.len != left.ptr + left.len)
		return APK_BLOB_NULL;

	return APK_BLOB_PTR_LEN(buffer.ptr, left.ptr - buffer.ptr);
}

static inline __attribute__((always_inline)) uint32_t rotl32(uint32_t x, int8_t r)
{
	return (x << r) | (x >> (32 - r));
}

static uint32_t murmur3_32(const void *pkey, uint32_t len, uint32_t seed)
{
	static const uint32_t c1 = 0xcc9e2d51;
	static const uint32_t c2 = 0x1b873593;
	const uint8_t *key = pkey;
	const int nblocks = len / 4;
	uint32_t k, h = seed;
	int i;

	for (i = 0; i < nblocks; i++, key += 4) {
		k  = get_unaligned32(key);
		k *= c1;
		k  = rotl32(k, 15);
		k *= c2;
		h ^= k;
		h  = rotl32(h, 13) * 5 + 0xe6546b64;
	}

	k = 0;
	switch (len & 3) {
	case 3:
		k ^= key[2] << 16;
	case 2:
		k ^= key[1] << 8;
	case 1:
		k ^= key[0];
		k *= c1;
		k  = rotl32(k, 15);
		k *= c2;
		h ^= k;
	}
	h ^= len;
	h ^= (h >> 16);
	h *= 0x85ebca6b;
	h ^= (h >> 13);
	h *= 0xc2b2ae35;
	h ^= (h >> 16);
	return h;
}

unsigned long apk_blob_hash_seed(apk_blob_t blob, unsigned long seed)
{
	return murmur3_32(blob.ptr, blob.len, seed);
}

unsigned long apk_blob_hash(apk_blob_t blob)
{
	return apk_blob_hash_seed(blob, 5381);
}

int apk_blob_compare(apk_blob_t a, apk_blob_t b)
{
	if (a.len == b.len)
		return memcmp(a.ptr, b.ptr, a.len);
	if (a.len < b.len)
		return -1;
	return 1;
}

int apk_blob_sort(apk_blob_t a, apk_blob_t b)
{
	int s = memcmp(a.ptr, b.ptr, min(a.len, b.len));
	if (s != 0) return s;
	return a.len - b.len;
}

int apk_blob_starts_with(apk_blob_t a, apk_blob_t b)
{
	if (a.len < b.len) return 0;
	return memcmp(a.ptr, b.ptr, b.len) == 0;
}

int apk_blob_ends_with(apk_blob_t a, apk_blob_t b)
{
	if (a.len < b.len) return 0;
	return memcmp(a.ptr+a.len-b.len, b.ptr, b.len) == 0;
}

int apk_blob_for_each_segment(apk_blob_t blob, const char *split,
			      int (*cb)(void *ctx, apk_blob_t blob), void *ctx)
{
	apk_blob_t l, r, s = APK_BLOB_STR(split);
	int rc;

	r = blob;
	while (apk_blob_split(r, s, &l, &r)) {
		rc = cb(ctx, l);
		if (rc != 0)
			return rc;
	}
	if (r.len > 0)
		return cb(ctx, r);
	return 0;
}

static unsigned char digitdecode[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0xff,
	   0,    1,    2,    3,    4,    5,    6,    7,
	   8,    9, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff,
	0xff,  0xa,  0xb,  0xc,  0xd,  0xe,  0xf, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff,  0xa,  0xb,  0xc,  0xd,  0xe,  0xf, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static inline int dx(unsigned char c)
{
	return digitdecode[c];
}

void apk_blob_push_blob(apk_blob_t *to, apk_blob_t literal)
{
	if (unlikely(APK_BLOB_IS_NULL(*to)))
		return;

	if (unlikely(to->len < literal.len)) {
		*to = APK_BLOB_NULL;
		return;
	}

	memcpy(to->ptr, literal.ptr, literal.len);
	to->ptr += literal.len;
	to->len -= literal.len;
}

static const char *xd = "0123456789abcdefghijklmnopqrstuvwxyz";

void apk_blob_push_uint(apk_blob_t *to, unsigned int value, int radix)
{
	char buf[64];
	char *ptr = &buf[sizeof(buf)-1];

	if (value == 0) {
		apk_blob_push_blob(to, APK_BLOB_STR("0"));
		return;
	}

	while (value != 0) {
		*(ptr--) = xd[value % radix];
		value /= radix;
	}

	apk_blob_push_blob(to, APK_BLOB_PTR_PTR(ptr+1, &buf[sizeof(buf)-1]));
}

void apk_blob_push_csum(apk_blob_t *to, struct apk_checksum *csum)
{
	switch (csum->type) {
	case APK_CHECKSUM_MD5:
		apk_blob_push_hexdump(to, APK_BLOB_CSUM(*csum));
		break;
	case APK_CHECKSUM_SHA1:
		apk_blob_push_blob(to, APK_BLOB_STR("Q1"));
		apk_blob_push_base64(to, APK_BLOB_CSUM(*csum));
		break;
	default:
		*to = APK_BLOB_NULL;
		break;
	}
}

static const char b64encode[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static inline void push_b64_chunk(unsigned char *to, const unsigned char *from, int len)
{
	to[0] = b64encode[from[0] >> 2];
	to[1] = b64encode[((from[0] & 0x03) << 4) | ((from[1] & 0xf0) >> 4)];
	to[2] = len < 2 ? '=' : b64encode[((from[1] & 0x0f) << 2) |
	                                  ((from[2] & 0xc0) >> 6)];
	to[3] = len < 3 ? '=' : b64encode[from[2] & 0x3f ];
}

void apk_blob_push_base64(apk_blob_t *to, apk_blob_t binary)
{
	unsigned char *src = (unsigned char *) binary.ptr;
	unsigned char *dst = (unsigned char *) to->ptr;
	int i, needed;

	if (unlikely(APK_BLOB_IS_NULL(*to)))
		return;

	needed = ((binary.len + 2) / 3) * 4;
	if (unlikely(to->len < needed)) {
		*to = APK_BLOB_NULL;
		return;
	}

	for (i = 0; i < binary.len / 3; i++, src += 3, dst += 4)
		push_b64_chunk(dst, src, 4);
	i = binary.len % 3;
	if (i != 0)
		push_b64_chunk(dst, src, i);
	to->ptr += needed;
	to->len -= needed;
}

void apk_blob_push_hexdump(apk_blob_t *to, apk_blob_t binary)
{
	char *d;
	int i;

	if (unlikely(APK_BLOB_IS_NULL(*to)))
		return;

	if (unlikely(to->len < binary.len * 2)) {
		*to = APK_BLOB_NULL;
		return;
	}

	for (i = 0, d = to->ptr; i < binary.len; i++) {
		*(d++) = xd[(binary.ptr[i] >> 4) & 0xf];
		*(d++) = xd[binary.ptr[i] & 0xf];
	}
	to->ptr = d;
	to->len -= binary.len * 2;
}

void apk_blob_push_fmt(apk_blob_t *to, const char *fmt, ...)
{
	va_list va;
	int n;

	if (unlikely(APK_BLOB_IS_NULL(*to)))
		return;

	va_start(va, fmt);
	n = vsnprintf(to->ptr, to->len, fmt, va);
	va_end(va);

	if (n >= 0 && n <= to->len) {
		to->ptr += n;
		to->len -= n;
	} else {
		*to = APK_BLOB_NULL;
	}
}

void apk_blob_pull_char(apk_blob_t *b, int expected)
{
	if (unlikely(APK_BLOB_IS_NULL(*b)))
		return;
	if (unlikely(b->len < 1 || b->ptr[0] != expected)) {
		*b = APK_BLOB_NULL;
		return;
	}
	b->ptr ++;
	b->len --;
}

unsigned int apk_blob_pull_uint(apk_blob_t *b, int radix)
{
	unsigned int val;
	int ch;

	val = 0;
	while (b->len && b->ptr[0] != 0) {
		ch = dx(b->ptr[0]);
		if (ch >= radix)
			break;
		val *= radix;
		val += ch;

		b->ptr++;
		b->len--;
	}

	return val;
}

void apk_blob_pull_csum(apk_blob_t *b, struct apk_checksum *csum)
{
	int encoding;

	if (unlikely(APK_BLOB_IS_NULL(*b))) goto fail;
	if (unlikely(b->len < 2)) goto fail;

	if (dx(b->ptr[0]) != 0xff) {
		/* Assume MD5 for backwards compatibility */
		csum->type = APK_CHECKSUM_MD5;
		apk_blob_pull_hexdump(b, APK_BLOB_CSUM(*csum));
		if (unlikely(APK_BLOB_IS_NULL(*b))) goto fail;
		return;
	}

	encoding = b->ptr[0];
	switch (b->ptr[1]) {
	case '1':
		csum->type = APK_CHECKSUM_SHA1;
		break;
	default:
		goto fail;
	}
	b->ptr += 2;
	b->len -= 2;

	switch (encoding) {
	case 'X':
		apk_blob_pull_hexdump(b, APK_BLOB_CSUM(*csum));
		break;
	case 'Q':
		apk_blob_pull_base64(b, APK_BLOB_CSUM(*csum));
		break;
	default:
	fail:
		*b = APK_BLOB_NULL;
		csum->type = APK_CHECKSUM_NONE;
		break;
	}
}

void apk_blob_pull_hexdump(apk_blob_t *b, apk_blob_t to)
{
	char *s, *d;
	int i, r, r1, r2;

	if (unlikely(APK_BLOB_IS_NULL(*b)))
		return;

	if (unlikely(to.len > b->len * 2))
		goto err;

	r = 0;
	for (i = 0, s = b->ptr, d = to.ptr; i < to.len; i++) {
		r |= r1 = dx(*(s++));
		r |= r2 = dx(*(s++));
		*(d++) = (r1 << 4) + r2;
	}
	if (unlikely(r == 0xff))
		goto err;
	b->ptr = s;
	b->len -= to.len * 2;
	return;
err:
	*b = APK_BLOB_NULL;
}

int apk_blob_pull_blob_match(apk_blob_t *b, apk_blob_t match)
{
	if (b->len < match.len) return 0;
	if (memcmp(b->ptr, match.ptr, match.len) != 0) return 0;
	b->ptr += match.len;
	b->len -= match.len;
	return 1;
}

static unsigned char b64decode[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
	0x3c, 0x3d, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff,
	0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
	0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
	0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
	0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
	0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff,

	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static inline __attribute__((always_inline))
int pull_b64_chunk(unsigned char *restrict to, const unsigned char *restrict from, int len)
{
	unsigned char tmp[4];
	int i, r = 0;

	for (i = 0; i < 4; i++) {
		tmp[i] = b64decode[from[i]];
		r |= tmp[i];
	}
	if (unlikely(r == 0xff))
		return -1;

	to[0] = (tmp[0] << 2 | tmp[1] >> 4);
	if (len > 1)
		to[1] = (tmp[1] << 4 | tmp[2] >> 2);
	else if (unlikely(from[2] != '='))
		return -1;
	if (len > 2)
		to[2] = (((tmp[2] << 6) & 0xc0) | tmp[3]);
	else if (unlikely(from[3] != '='))
		return -1;
	return 0;
}

void apk_blob_pull_base64(apk_blob_t *b, apk_blob_t to)
{
	unsigned char tmp[4];
	unsigned char *restrict src = (unsigned char *) b->ptr;
	unsigned char *restrict dst = (unsigned char *) to.ptr;
	unsigned char *dend;
	int r, needed;

	if (unlikely(APK_BLOB_IS_NULL(*b)))
		return;

	needed = ((to.len + 2) / 3) * 4;
	if (unlikely(b->len < needed)) goto err;

	r = 0;
	dend = dst + to.len - 2;
	for (; dst < dend; src += 4, dst += 3) {
		r |= tmp[0] = b64decode[src[0]];
		r |= tmp[1] = b64decode[src[1]];
		r |= tmp[2] = b64decode[src[2]];
		r |= tmp[3] = b64decode[src[3]];
		dst[0] = (tmp[0] << 2 | tmp[1] >> 4);
		dst[1] = (tmp[1] << 4 | tmp[2] >> 2);
		dst[2] = (((tmp[2] << 6) & 0xc0) | tmp[3]);
	}
	if (unlikely(r == 0xff)) goto err;

	dend += 2;
	if (dst != dend &&
	    pull_b64_chunk(dst, src, dend - dst) != 0)
		goto err;

	b->ptr += needed;
	b->len -= needed;
	return;
err:
	*b = APK_BLOB_NULL;
}
