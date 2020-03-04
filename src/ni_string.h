/* ni_string.h -- A C dynamic strings library
 *
 * Copyright (c) 2020-2030, Lei Wang <wanglei_gmgc@163.com>
 * All rights reserved.
 *
 */

#ifndef _NI_STRING_H_
#define _NI_STRING_H_

#define NI_STRING_MAX_PREALLOC      (1024*1024)
const char *NI_STRING_NOINIT;

#include <sys/types.h>
#include <stdarg.h>
#include <stdint.h>

typedef char *ni_string;

/* Note: sdshdr5 is never used, we just access the flags byte directly.
 * However is here to document the layout of type 5 SDS strings. */
struct __attribute__ ((__packed__)) ni_string_hdr5 {
    unsigned char flags; /* 3 lsb of type, and 5 msb of string length */
    char buf[];
};
struct __attribute__ ((__packed__)) ni_string_hdr8 {
    uint8_t len; /* used */
    uint8_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__ ((__packed__)) ni_string_hdr16 {
    uint16_t len; /* used */
    uint16_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__ ((__packed__)) ni_string_hdr32 {
    uint32_t len; /* used */
    uint32_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__ ((__packed__)) ni_string_hdr64 {
    uint64_t len; /* used */
    uint64_t alloc; /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};

#define NI_STRING_TYPE_5        0
#define NI_STRING_TYPE_8        1
#define NI_STRING_TYPE_16       2
#define NI_STRING_TYPE_32       3
#define NI_STRING_TYPE_64       4
#define NI_STRING_TYPE_MASK     7
#define NI_STRING_TYPE_BITS     3
#define NI_STRING_HDR_VAR(T, s) struct ni_string_hdr##T *sh = (void*)((s) - (sizeof(struct ni_string_hdr##T)));
#define NI_STRING_HDR(T, s) ((struct ni_string_hdr##T *)((s) - (sizeof(struct ni_string_hdr##T))))
#define NI_STRING_TYPE_5_LEN(f) ((f) >> NI_STRING_TYPE_BITS)

static inline size_t ni_string_len(const ni_string s) {
    unsigned char flags = s[-1];
    switch (flags & NI_STRING_TYPE_MASK) {
        case NI_STRING_TYPE_5:
            return NI_STRING_TYPE_5_LEN(flags);
        case NI_STRING_TYPE_8:
            return NI_STRING_HDR(8, s)->len;
        case NI_STRING_TYPE_16:
            return NI_STRING_HDR(16, s)->len;
        case NI_STRING_TYPE_32:
            return NI_STRING_HDR(32, s)->len;
        case NI_STRING_TYPE_64:
            return NI_STRING_HDR(64, s)->len;
    }
    return 0;
}

static inline size_t ni_string_avail(const ni_string s) {
    unsigned char flags = s[-1];
    switch (flags & NI_STRING_TYPE_MASK) {
        case NI_STRING_TYPE_5: {
            return 0;
        }
        case NI_STRING_TYPE_8: {
            NI_STRING_HDR_VAR(8, s);
            return sh->alloc - sh->len;
        }
        case NI_STRING_TYPE_16: {
            NI_STRING_HDR_VAR(16, s);
            return sh->alloc - sh->len;
        }
        case NI_STRING_TYPE_32: {
            NI_STRING_HDR_VAR(32, s);
            return sh->alloc - sh->len;
        }
        case NI_STRING_TYPE_64: {
            NI_STRING_HDR_VAR(64, s);
            return sh->alloc - sh->len;
        }
    }
    return 0;
}

static inline void ni_string_set_len(ni_string s, size_t newlen) {
    unsigned char flags = s[-1];
    switch (flags & NI_STRING_TYPE_MASK) {
        case NI_STRING_TYPE_5: {
                unsigned char *fp = ((unsigned char*)s)-1;
                *fp = NI_STRING_TYPE_5 | (newlen << NI_STRING_TYPE_BITS);
            }
            break;
        case NI_STRING_TYPE_8:
            NI_STRING_HDR(8, s)->len = newlen;
            break;
        case NI_STRING_TYPE_16:
            NI_STRING_HDR(16, s)->len = newlen;
            break;
        case NI_STRING_TYPE_32:
            NI_STRING_HDR(32, s)->len = newlen;
            break;
        case NI_STRING_TYPE_64:
            NI_STRING_HDR(64, s)->len = newlen;
            break;
    }
}

static inline void ni_string_inc_len(ni_string s, size_t inc) {
    unsigned char flags = s[-1];
    switch (flags & NI_STRING_TYPE_MASK) {
        case NI_STRING_TYPE_5: {
                unsigned char *fp = ((unsigned char*)s) - 1;
                unsigned char newlen = NI_STRING_TYPE_5_LEN(flags) + inc;
                *fp = NI_STRING_TYPE_5 | (newlen << NI_STRING_TYPE_BITS);
            }
            break;
        case NI_STRING_TYPE_8:
            NI_STRING_HDR(8, s)->len += inc;
            break;
        case NI_STRING_TYPE_16:
            NI_STRING_HDR(16, s)->len += inc;
            break;
        case NI_STRING_TYPE_32:
            NI_STRING_HDR(32, s)->len += inc;
            break;
        case NI_STRING_TYPE_64:
            NI_STRING_HDR(64, s)->len += inc;
            break;
    }
}

/* ni_string_alloc() = ni_string_avail() + ni_string_len() */
static inline size_t ni_string_alloc(const ni_string s) {
    unsigned char flags = s[-1];
    switch (flags & NI_STRING_TYPE_MASK) {
        case NI_STRING_TYPE_5:
            return NI_STRING_TYPE_5_LEN(flags);
        case NI_STRING_TYPE_8:
            return NI_STRING_HDR(8, s)->alloc;
        case NI_STRING_TYPE_16:
            return NI_STRING_HDR(16, s)->alloc;
        case NI_STRING_TYPE_32:
            return NI_STRING_HDR(32, s)->alloc;
        case NI_STRING_TYPE_64:
            return NI_STRING_HDR(64, s)->alloc;
    }
    return 0;
}

static inline void ni_string_set_alloc(ni_string s, size_t newlen) {
    unsigned char flags = s[-1];
    switch (flags & NI_STRING_TYPE_MASK) {
        case NI_STRING_TYPE_5:
            /* Nothing to do, this type has no total allocation info. */
            break;
        case NI_STRING_TYPE_8:
            NI_STRING_HDR(8, s)->alloc = newlen;
            break;
        case NI_STRING_TYPE_16:
            NI_STRING_HDR(16, s)->alloc = newlen;
            break;
        case NI_STRING_TYPE_32:
            NI_STRING_HDR(32, s)->alloc = newlen;
            break;
        case NI_STRING_TYPE_64:
            NI_STRING_HDR(64, s)->alloc = newlen;
            break;
    }
}

ni_string ni_string_new_len(const void *init, size_t initlen);
ni_string ni_string_new(const char *init);
ni_string ni_string_empty(void);
ni_string ni_string_dup(const ni_string s);
void ni_string_free(ni_string s);
ni_string ni_string_grow_zero(ni_string s, size_t len);
ni_string ni_string_cat_len(ni_string s, const void *t, size_t len);
ni_string ni_string_cat(ni_string s, const char *t);
ni_string ni_string_cat_ni_string(ni_string s, const ni_string t);
ni_string ni_string_cpy_len(ni_string s, const char *t, size_t len);
ni_string ni_string_cpy(ni_string s, const char *t);

ni_string ni_string_cat_vprintf(ni_string s, const char *fmt, va_list ap);
#ifdef __GNUC__
ni_string ni_string_cat_printf(ni_string s, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
#else
ni_string ni_string_cat_printf(ni_string s, const char *fmt, ...);
#endif

ni_string ni_string_cat_fmt(ni_string s, char const *fmt, ...);
ni_string ni_string_trim(ni_string s, const char *cset);
void ni_string_range(ni_string s, ssize_t start, ssize_t end);
void ni_string_update_len(ni_string s);
void ni_string_clear(ni_string s);
int ni_string_cmp(const ni_string s1, const ni_string s2);
ni_string *ni_string_split_len(const char *s, ssize_t len, const char *sep, int seplen, int *count);
void ni_string_free_split_res(ni_string *tokens, int count);
void ni_string_tolower(ni_string s);
void ni_string_toupper(ni_string s);
ni_string ni_string_from_longlong(long long value);
ni_string ni_string_cat_repr(ni_string s, const char *p, size_t len);
ni_string *ni_string_split_args(const char *line, int *argc);
ni_string ni_string_map_chars(ni_string s, const char *from, const char *to, size_t setlen);
ni_string ni_string_join(char **argv, int argc, char *sep);
ni_string ni_string_join_ni_string(ni_string *argv, int argc, const char *sep, size_t seplen);

/* Low level functions exposed to the user API */
ni_string ni_string_make_room_for(ni_string s, size_t addlen);
void ni_string_incr_len(ni_string s, ssize_t incr);
ni_string ni_string_remove_free_space(ni_string s);
size_t ni_string_alloc_size(ni_string s);
void *ni_string_alloc_ptr(ni_string s);

/* Export the allocator used by ni_string to the program using ni_string.
 * Sometimes the program ni_string is linked to, may use a different set of
 * allocators, but may want to allocate or free things that ni_string will
 * respectively free or allocate. */
void *ni_string_malloc(size_t size);
void *ni_string_realloc(void *ptr, size_t size);
void ni_string_ptr_free(void *ptr);

#endif /* _NI_STRING_H_ */
