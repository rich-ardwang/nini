/* ni_string.c -- A C dynamic strings library
 *
 * Copyright (c) 2020-2030, Lei Wang <wanglei_gmgc@163.com>
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include "ni_string.h"
#include "ni_malloc.h"

const char *NI_STRING_NOINIT = "NI_STRING_NOINIT";

static inline int ni_string_hdr_size(char type) {
    switch (type & NI_STRING_TYPE_MASK) {
        case NI_STRING_TYPE_5:
            return sizeof(struct ni_string_hdr5);
        case NI_STRING_TYPE_8:
            return sizeof(struct ni_string_hdr8);
        case NI_STRING_TYPE_16:
            return sizeof(struct ni_string_hdr16);
        case NI_STRING_TYPE_32:
            return sizeof(struct ni_string_hdr32);
        case NI_STRING_TYPE_64:
            return sizeof(struct ni_string_hdr64);
    }
    return 0;
}

static inline char ni_string_req_type(size_t ni_string_size) {
    if (ni_string_size < 1<<5) //32
        return NI_STRING_TYPE_5;
    if (ni_string_size < 1<<8) //256
        return NI_STRING_TYPE_8;
    if (ni_string_size < 1<<16) //65536
        return NI_STRING_TYPE_16;
#if (LONG_MAX == LLONG_MAX)
    if (ni_string_size < 1ll<<32)
        return NI_STRING_TYPE_32;
    return NI_STRING_TYPE_64;
#else
    return NI_STRING_TYPE_32;
#endif
}

/* Create a new ni_string with the content specified by the 'init' pointer
 * and 'initlen'.
 * If NULL is used for 'init' the string is initialized with zero bytes.
 * If NI_STRING_NOINIT is used, the buffer is left uninitialized;
 *
 * The string is always null-termined (all the ni_string strings are, always) so
 * even if you create an ni_string string with:
 *
 * mystring = ni_stringnewlen("abc",3);
 *
 * You can print the string with printf() as there is an implicit \0 at the
 * end of the string. However the string is binary safe and can contain
 * \0 characters in the middle, as the length is stored in the ni_string header. */
ni_string ni_string_new_len(const void *init, size_t initlen) {
    void        *sh;
    ni_string   s;
    char type = ni_string_req_type(initlen);
    /* Empty strings are usually created in order to append. Use type 8
     * since type 5 is not good at this. */
    if (type == NI_STRING_TYPE_5 && initlen == 0)
        type = NI_STRING_TYPE_8;
    int hdrlen = ni_string_hdr_size(type);
    unsigned char *fp; /* flags pointer. */

    sh = ni_string_malloc(hdrlen + initlen + 1);
    if (init == NI_STRING_NOINIT)
        init = NULL;
    else if (!init)
        memset(sh, 0, hdrlen + initlen + 1);
    if (sh == NULL) return NULL;
    s = (char*)sh + hdrlen;
    fp = ((unsigned char*)s) - 1;
    switch(type) {
        case NI_STRING_TYPE_5: {
            *fp = type | (initlen << NI_STRING_TYPE_BITS);
            break;
        }
        case NI_STRING_TYPE_8: {
            NI_STRING_HDR_VAR(8, s);
            sh->len = initlen;
            sh->alloc = initlen;
            *fp = type;
            break;
        }
        case NI_STRING_TYPE_16: {
            NI_STRING_HDR_VAR(16, s);
            sh->len = initlen;
            sh->alloc = initlen;
            *fp = type;
            break;
        }
        case NI_STRING_TYPE_32: {
            NI_STRING_HDR_VAR(32, s);
            sh->len = initlen;
            sh->alloc = initlen;
            *fp = type;
            break;
        }
        case NI_STRING_TYPE_64: {
            NI_STRING_HDR_VAR(64, s);
            sh->len = initlen;
            sh->alloc = initlen;
            *fp = type;
            break;
        }
    }
    if (initlen && init)
        memcpy(s, init, initlen);
    s[initlen] = '\0';
    return s;
}

/* Create an empty (zero length) ni_string. Even in this case the string
 * always has an implicit null term. */
ni_string ni_string_empty(void) {
    return ni_string_new_len("", 0);
}

/* Create a new ni_string starting from a null terminated C string. */
ni_string ni_string_new(const char *init) {
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return ni_string_new_len(init, initlen);
}

/* Duplicate an ni_string. */
ni_string ni_string_dup(const ni_string s) {
    return ni_string_new_len(s, ni_string_len(s));
}

/* Free an ni_string. No operation is performed if 's' is NULL. */
void ni_string_free(ni_string s) {
    if (s == NULL) return;
    ni_string_ptr_free((char*)s - ni_string_hdr_size(s[-1]));
}

/* Set the ni_string length to the length as obtained with strlen(), so
 * considering as content only up to the first null term character.
 *
 * This function is useful when the ni_string string is hacked manually in some
 * way, like in the following example:
 *
 * s = ni_stringnew("foobar");
 * s[2] = '\0';
 * ni_string_update_len(s);
 * printf("%d\n", ni_string_len(s));
 *
 * The output will be "2", but if we comment out the call to ni_string_update_len()
 * the output will be "6" as the string was modified but the logical length
 * remains 6 bytes. */
void ni_string_update_len(ni_string s) {
    size_t reallen = strlen(s);
    ni_string_set_len(s, reallen);
}

/* Modify an ni_string in-place to make it empty (zero length).
 * However all the existing buffer is not discarded but set as free space
 * so that next append operations will not require allocations up to the
 * number of bytes previously available. */
void ni_string_clear(ni_string s) {
    ni_string_set_len(s, 0);
    s[0] = '\0';
}

/* Enlarge the free space at the end of the ni_string so that the caller
 * is sure that after calling this function can overwrite up to addlen
 * bytes after the end of the string, plus one more byte for null term.
 *
 * Note: this does not change the *length* of the ni_string string as returned
 * by ni_string_len(), but only the free buffer space we have. */
ni_string ni_string_make_room_for(ni_string s, size_t addlen) {
    void *sh, *newsh;
    size_t avail = ni_string_avail(s);
    size_t len, newlen;
    char type, oldtype = s[-1] & NI_STRING_TYPE_MASK;
    int hdrlen;

    /* Return ASAP if there is enough space left. */
    if (avail >= addlen) return s;

    len = ni_string_len(s);
    sh = (char*)s - ni_string_hdr_size(oldtype);
    newlen = (len + addlen);
    if (newlen < NI_STRING_MAX_PREALLOC)
        newlen *= 2;
    else
        newlen += NI_STRING_MAX_PREALLOC;

    type = ni_string_req_type(newlen);

    /* Don't use type 5: the user is appending to the string and type 5 is
     * not able to remember empty space, so ni_string_make_room_for() must be called
     * at every appending operation. */
    if (type == NI_STRING_TYPE_5) type = NI_STRING_TYPE_8;

    hdrlen = ni_string_hdr_size(type);
    if (oldtype == type) {
        newsh = ni_string_realloc(sh, hdrlen + newlen + 1);
        if (newsh == NULL) return NULL;
        s = (char*)newsh + hdrlen;
    } else {
        /* Since the header size changes, need to move the string forward,
         * and can't use realloc */
        newsh = ni_string_malloc(hdrlen + newlen + 1);
        if (newsh == NULL) return NULL;
        memcpy((char*)newsh + hdrlen, s, len + 1);
        ni_string_ptr_free(sh);
        s = (char*)newsh + hdrlen;
        s[-1] = type;
        ni_string_set_len(s, len);
    }
    ni_string_set_alloc(s, newlen);
    return s;
}

/* Reallocate the ni_string so that it has no free space at the end. The
 * contained string remains not altered, but next concatenation operations
 * will require a reallocation.
 *
 * After the call, the passed ni_string string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
ni_string ni_string_remove_free_space(ni_string s) {
    void *sh, *newsh;
    char type, oldtype = s[-1] & NI_STRING_TYPE_MASK;
    int hdrlen, oldhdrlen = ni_string_hdr_size(oldtype);
    size_t len = ni_string_len(s);
    size_t avail = ni_string_avail(s);
    sh = (char*)s - oldhdrlen;

    /* Return ASAP if there is no space left. */
    if (avail == 0) return s;

    /* Check what would be the minimum NI_STRING header that is just good enough to
     * fit this string. */
    type = ni_string_req_type(len);
    hdrlen = ni_string_hdr_size(type);

    /* If the type is the same, or at least a large enough type is still
     * required, we just realloc(), letting the allocator to do the copy
     * only if really needed. Otherwise if the change is huge, we manually
     * reallocate the string to use the different header type. */
    if (oldtype == type || type > NI_STRING_TYPE_8) {
        newsh = ni_string_realloc(sh, oldhdrlen + len + 1);
        if (newsh == NULL) return NULL;
        s = (char*)newsh + oldhdrlen;
    } else {
        newsh = ni_string_malloc(hdrlen + len + 1);
        if (newsh == NULL) return NULL;
        memcpy((char*)newsh + hdrlen, s, len + 1);
        ni_string_ptr_free(sh);
        s = (char*)newsh + hdrlen;
        s[-1] = type;
        ni_string_set_len(s, len);
    }
    ni_string_set_alloc(s, len);
    return s;
}

/* Return the total size of the allocation of the specified ni_string,
 * including:
 * 1) The ni_string header before the pointer.
 * 2) The string.
 * 3) The free buffer at the end if any.
 * 4) The implicit null term.
 */
size_t ni_string_alloc_size(ni_string s) {
    size_t alloc = ni_string_alloc(s);
    return ni_string_hdr_size(s[-1]) + alloc + 1;
}

/* Return the pointer of the actual ni_string allocation (normally ni_strings
 * are referenced by the start of the string buffer). */
void *ni_string_alloc_ptr(ni_string s) {
    return (void*)(s - ni_string_hdr_size(s[-1]));
}

/* Increment the ni_string length and decrements the left free space at the
 * end of the string according to 'incr'. Also set the null term
 * in the new end of the string.
 *
 * This function is used in order to fix the string length after the
 * user calls ni_string_make_room_for(), writes something after the end of
 * the current string, and finally needs to set the new length.
 *
 * Note: it is possible to use a negative increment in order to
 * right-trim the string.
 *
 * Usage example:
 *
 * Using ni_string_incr_len() and ni_string_make_room_for() it is possible to mount the
 * following schema, to cat bytes coming from the kernel to the end of an
 * ni_string string without copying into an intermediate buffer:
 *
 * oldlen = ni_string_len(s);
 * s = ni_string_make_room_for(s, BUFFER_SIZE);
 * nread = read(fd, s + oldlen, BUFFER_SIZE);
 * ... check for nread <= 0 and handle it ...
 * ni_string_incr_len(s, nread);
 */
void ni_string_incr_len(ni_string s, ssize_t incr) {
    unsigned char flags = s[-1];
    size_t len;
    switch (flags & NI_STRING_TYPE_MASK) {
        case NI_STRING_TYPE_5: {
            unsigned char *fp = ((unsigned char*)s) - 1;
            unsigned char oldlen = NI_STRING_TYPE_5_LEN(flags);
            assert((incr > 0 && oldlen + incr < 32) || (incr < 0 && oldlen >= (unsigned int)(-incr)));
            *fp = NI_STRING_TYPE_5 | ((oldlen + incr) << NI_STRING_TYPE_BITS);
            len = oldlen + incr;
            break;
        }
        case NI_STRING_TYPE_8: {
            NI_STRING_HDR_VAR(8, s);
            assert((incr >= 0 && sh->alloc-sh->len >= incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
            len = (sh->len += incr);
            break;
        }
        case NI_STRING_TYPE_16: {
            NI_STRING_HDR_VAR(16, s);
            assert((incr >= 0 && sh->alloc-sh->len >= incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
            len = (sh->len += incr);
            break;
        }
        case NI_STRING_TYPE_32: {
            NI_STRING_HDR_VAR(32, s);
            assert((incr >= 0 && sh->alloc-sh->len >= (unsigned int)incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
            len = (sh->len += incr);
            break;
        }
        case NI_STRING_TYPE_64: {
            NI_STRING_HDR_VAR(64, s);
            assert((incr >= 0 && sh->alloc-sh->len >= (uint64_t)incr) || (incr < 0 && sh->len >= (uint64_t)(-incr)));
            len = (sh->len += incr);
            break;
        }
        default: len = 0; /* Just to avoid compilation warnings. */
    }
    s[len] = '\0';
}

/* Grow the ni_string to have the specified length. Bytes that were not part of
 * the original length of the ni_string will be set to zero.
 *
 * if the specified length is smaller than the current length, no operation
 * is performed. */
ni_string ni_string_grow_zero(ni_string s, size_t len) {
    size_t curlen = ni_string_len(s);

    if (len <= curlen) return s;
    s = ni_string_make_room_for(s, len - curlen);
    if (s == NULL) return NULL;

    /* Make sure added region doesn't contain garbage */
    memset(s + curlen, 0, (len - curlen + 1)); /* also set trailing \0 byte */
    ni_string_set_len(s, len);
    return s;
}

/* Append the specified binary-safe string pointed by 't' of 'len' bytes to the
 * end of the specified ni_string 's'.
 *
 * After the call, the passed ni_string string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
ni_string ni_string_cat_len(ni_string s, const void *t, size_t len) {
    size_t curlen = ni_string_len(s);
    s = ni_string_make_room_for(s, len);
    if (s == NULL) return NULL;
    memcpy(s + curlen, t, len);
    ni_string_set_len(s, curlen + len);
    s[curlen + len] = '\0';
    return s;
}

/* Append the specified null termianted C string to the ni_string 's'.
 *
 * After the call, the passed ni_string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
ni_string ni_string_cat(ni_string s, const char *t) {
    return ni_string_cat_len(s, t, strlen(t));
}

/* Append the specified ni_string 't' to the existing ni_string 's'.
 *
 * After the call, the modified ni_string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
ni_string ni_string_cat_ni_string(ni_string s, const ni_string t) {
    return ni_string_cat_len(s, t, ni_string_len(t));
}

/* Destructively modify the ni_string 's' to hold the specified binary
 * safe string pointed by 't' of length 'len' bytes. */
ni_string ni_string_cpy_len(ni_string s, const char *t, size_t len) {
    if (ni_string_alloc(s) < len) {
        s = ni_string_make_room_for(s, len - ni_string_len(s));
        if (s == NULL) return NULL;
    }
    memcpy(s, t, len);
    s[len] = '\0';
    ni_string_set_len(s, len);
    return s;
}

/* Like ni_string_cpy_len() but 't' must be a null-termined string so that the length
 * of the string is obtained with strlen(). */
ni_string ni_string_cpy(ni_string s, const char *t) {
    return ni_string_cpy_len(s, t, strlen(t));
}

/* Helper for ni_string_cat_longlong() doing the actual number -> string
 * conversion. 's' must point to a string with room for at least
 * NI_STRING_LLSTR_SIZE bytes.
 *
 * The function returns the length of the null-terminated string
 * representation stored at 's'. */
#define NI_STRING_LLSTR_SIZE 21
int ni_string_ll2str(char *s, long long value) {
    char *p, aux;
    unsigned long long v;
    size_t l;

    /* Generate the string representation, this method produces
     * an reversed string. */
    v = (value < 0) ? -value : value;
    p = s;
    do {
        *p++ = '0' + (v % 10);
        v /= 10;
    } while(v);
    if (value < 0) *p++ = '-';

    /* Compute length and add null term. */
    l = p - s;
    *p = '\0';

    /* Reverse the string. */
    p--;
    while(s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

/* Identical ni_string_ll2str(), but for unsigned long long type. */
int ni_string_ull2str(char *s, unsigned long long v) {
    char *p, aux;
    size_t l;

    /* Generate the string representation, this method produces
     * an reversed string. */
    p = s;
    do {
        *p++ = '0' + (v % 10);
        v /= 10;
    } while(v);

    /* Compute length and add null term. */
    l = p - s;
    *p = '\0';

    /* Reverse the string. */
    p--;
    while(s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

/* Create an ni_string from a long long value. It is much faster than:
 *
 * ni_string_cat_printf(ni_string_empty(),"%lld\n", value);
 */
ni_string ni_string_from_longlong(long long value) {
    char buf[NI_STRING_LLSTR_SIZE];
    int len = ni_string_ll2str(buf, value);
    return ni_string_new_len(buf, len);
}

/* Like ni_string_cat_printf() but gets va_list instead of being variadic. */
ni_string ni_string_cat_vprintf(ni_string s, const char *fmt, va_list ap) {
    va_list cpy;
    char staticbuf[1024], *buf = staticbuf, *t;
    size_t buflen = strlen(fmt) * 2;

    /* We try to start using a static buffer for speed.
     * If not possible we revert to heap allocation. */
    if (buflen > sizeof(staticbuf)) {
        buf = ni_string_malloc(buflen);
        if (buf == NULL) return NULL;
    } else {
        buflen = sizeof(staticbuf);
    }

    /* Try with buffers two times bigger every time we fail to
     * fit the string in the current buffer size. */
    while(1) {
        buf[buflen-2] = '\0';
        va_copy(cpy, ap);
        vsnprintf(buf, buflen, fmt, cpy);
        va_end(cpy);
        if (buf[buflen - 2] != '\0') {
            if (buf != staticbuf) ni_string_ptr_free(buf);
            buflen *= 2;
            buf = ni_string_malloc(buflen);
            if (buf == NULL) return NULL;
            continue;
        }
        break;
    }

    /* Finally concat the obtained string to the ni_string and return it. */
    t = ni_string_cat(s, buf);
    if (buf != staticbuf) ni_string_ptr_free(buf);
    return t;
}

/* Append to the ni_string 's' a string obtained using printf-alike format
 * specifier.
 *
 * After the call, the modified ni_string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = ni_string_new("Sum is: ");
 * s = ni_string_cat_printf(s, "%d + %d = %d", a, b, a + b).
 *
 * Often you need to create a string from scratch with the printf-alike
 * format. When this is the need, just use ni_string_empty() as the target string:
 *
 * s = ni_string_cat_printf(ni_string_empty(), "... your format ...", args);
 */
ni_string ni_string_cat_printf(ni_string s, const char *fmt, ...) {
    va_list ap;
    char *t;
    va_start(ap, fmt);
    t = ni_string_cat_vprintf(s, fmt, ap);
    va_end(ap);
    return t;
}

/* This function is similar to ni_string_cat_printf, but much faster as it does
 * not rely on sprintf() family functions implemented by the libc that
 * are often very slow. Moreover directly handling the ni_string as
 * new data is concatenated provides a performance improvement.
 *
 * However this function only handles an incompatible subset of printf-alike
 * format specifiers:
 *
 * %s - C String
 * %S - ni_string
 * %i - signed int
 * %I - 64 bit signed integer (long long, int64_t)
 * %u - unsigned int
 * %U - 64 bit unsigned integer (unsigned long long, uint64_t)
 * %% - Verbatim "%" character.
 */
ni_string ni_string_cat_fmt(ni_string s, char const *fmt, ...) {
    size_t initlen = ni_string_len(s);
    const char *f = fmt;
    long i;
    va_list ap;

    va_start(ap, fmt);
    f = fmt;    /* Next format specifier byte to process. */
    i = initlen; /* Position of the next byte to write to dest str. */
    while (*f) {
        char next, *str;
        size_t l;
        long long num;
        unsigned long long unum;

        /* Make sure there is always space for at least 1 char. */
        if (0 == ni_string_avail(s)) {
            s = ni_string_make_room_for(s, 1);
        }

        switch (*f) {
        case '%':
            next = *(f + 1);
            f++;
            switch (next) {
            case 's':
            case 'S':
                str = va_arg(ap, char*);
                l = (next == 's') ? strlen(str) : ni_string_len(str);
                if (ni_string_avail(s) < l) {
                    s = ni_string_make_room_for(s, l);
                }
                memcpy(s + i, str, l);
                ni_string_incr_len(s, l);
                i += l;
                break;
            case 'i':
            case 'I':
                if (next == 'i')
                    num = va_arg(ap, int);
                else
                    num = va_arg(ap, long long);
                {
                    char buf[NI_STRING_LLSTR_SIZE];
                    l = ni_string_ll2str(buf, num);
                    if (ni_string_avail(s) < l) {
                        s = ni_string_make_room_for(s, l);
                    }
                    memcpy(s + i, buf, l);
                    ni_string_incr_len(s, l);
                    i += l;
                }
                break;
            case 'u':
            case 'U':
                if (next == 'u')
                    unum = va_arg(ap, unsigned int);
                else
                    unum = va_arg(ap, unsigned long long);
                {
                    char buf[NI_STRING_LLSTR_SIZE];
                    l = ni_string_ull2str(buf, unum);
                    if (ni_string_avail(s) < l) {
                        s = ni_string_make_room_for(s, l);
                    }
                    memcpy(s + i, buf, l);
                    ni_string_incr_len(s, l);
                    i += l;
                }
                break;
            default: /* Handle %% and generally %<unknown>. */
                s[i++] = next;
                ni_string_incr_len(s, 1);
                break;
            }
            break;
        default:
            s[i++] = *f;
            ni_string_incr_len(s, 1);
            break;
        }
        f++;
    }
    va_end(ap);

    /* Add null-term */
    s[i] = '\0';
    return s;
}

/* Remove the part of the string from left and from right composed just of
 * contiguous characters found in 'cset', that is a null terminted C string.
 *
 * After the call, the modified ni_string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = ni_string_new("AA...AA.a.aa.aHelloWorld     :::");
 * s = ni_string_trim(s, "Aa. :");
 * printf("%s\n", s);
 *
 * Output will be just "HelloWorld".
 */
ni_string ni_string_trim(ni_string s, const char *cset) {
    char *start, *end, *sp, *ep;
    size_t len;

    sp = start = s;
    ep = end = s + ni_string_len(s) - 1;
    while (sp <= end && strchr(cset, *sp)) sp++;
    while (ep > sp && strchr(cset, *ep)) ep--;
    len = (sp > ep) ? 0 : ((ep - sp) + 1);
    if (s != sp) memmove(s, sp, len);
    s[len] = '\0';
    ni_string_set_len(s, len);
    return s;
}

/* Turn the string into a smaller (or equal) string containing only the
 * substring specified by the 'start' and 'end' indexes.
 *
 * start and end can be negative, where -1 means the last character of the
 * string, -2 the penultimate character, and so forth.
 *
 * The interval is inclusive, so the start and end characters will be part
 * of the resulting string.
 *
 * The string is modified in-place.
 *
 * Example:
 *
 * s = ni_string_new("Hello World");
 * ni_string_range(s, 1, -1); => "ello World"
 */
void ni_string_range(ni_string s, ssize_t start, ssize_t end) {
    size_t newlen, len = ni_string_len(s);

    if (len == 0) return;
    if (start < 0) {
        start = len + start;
        if (start < 0) start = 0;
    }
    if (end < 0) {
        end = len + end;
        if (end < 0) end = 0;
    }
    newlen = (start > end) ? 0 : (end - start) + 1;
    if (newlen != 0) {
        if (start >= (ssize_t)len) {
            newlen = 0;
        } else if (end >= (ssize_t)len) {
            end = len - 1;
            newlen = (start > end) ? 0 : (end - start) + 1;
        }
    } else {
        start = 0;
    }
    if (start && newlen) memmove(s, s + start, newlen);
    s[newlen] = 0;
    ni_string_set_len(s, newlen);
}

/* Apply tolower() to every character of the ni_string 's'. */
void ni_string_tolower(ni_string s) {
    size_t len = ni_string_len(s), j;

    for (j = 0; j < len; j++) s[j] = tolower(s[j]);
}

/* Apply toupper() to every character of the ni_string 's'. */
void ni_string_toupper(ni_string s) {
    size_t len = ni_string_len(s), j;

    for (j = 0; j < len; j++) s[j] = toupper(s[j]);
}

/* Compare two ni_strings s1 and s2 with memcmp().
 *
 * Return value:
 *
 *     positive if s1 > s2.
 *     negative if s1 < s2.
 *     0 if s1 and s2 are exactly the same binary string.
 *
 * If two strings share exactly the same prefix, but one of the two has
 * additional characters, the longer string is considered to be greater than
 * the smaller one. */
int ni_string_cmp(const ni_string s1, const ni_string s2) {
    size_t l1, l2, minlen;
    int cmp;

    l1 = ni_string_len(s1);
    l2 = ni_string_len(s2);
    minlen = (l1 < l2) ? l1 : l2;
    cmp = memcmp(s1, s2, minlen);
    if (cmp == 0) return l1 > l2 ? 1 : (l1 < l2 ? -1 : 0);
    return cmp;
}

/* Split 's' with separator in 'sep'. An array
 * of ni_strings is returned. *count will be set
 * by reference to the number of tokens returned.
 *
 * On out of memory, zero length string, zero length
 * separator, NULL is returned.
 *
 * Note that 'sep' is able to split a string using
 * a multi-character separator. For example
 * ni_string_split("foo_-_bar", "_-_"); will return two
 * elements "foo" and "bar".
 *
 * This version of the function is binary-safe but
 * requires length arguments. ni_string_split() is just the
 * same function but for zero-terminated strings.
 */
ni_string *ni_string_split_len(const char *s, ssize_t len, const char *sep, int seplen, int *count) {
    int elements = 0, slots = 5;
    long start = 0, j;
    ni_string *tokens;

    if (seplen < 1 || len < 0) return NULL;

    tokens = ni_string_malloc(sizeof(ni_string) * slots);
    if (tokens == NULL) return NULL;

    if (len == 0) {
        *count = 0;
        return tokens;
    }
    for (j = 0; j < (len - (seplen - 1)); j++) {
        /* make sure there is room for the next element and the final one */
        if (slots < elements + 2) {
            ni_string *newtokens;

            slots *= 2;
            newtokens = ni_string_realloc(tokens, sizeof(ni_string) * slots);
            if (newtokens == NULL) goto cleanup;
            tokens = newtokens;
        }
        /* search the separator */
        if ((seplen == 1 && *(s + j) == sep[0]) || (memcmp(s + j, sep, seplen) == 0)) {
            tokens[elements] = ni_string_new_len(s + start, j - start);
            if (tokens[elements] == NULL) goto cleanup;
            elements++;
            start = j + seplen;
            j = j + seplen - 1; /* skip the separator */
        }
    }
    /* Add the final element. We are sure there is room in the tokens array. */
    tokens[elements] = ni_string_new_len(s + start, len - start);
    if (tokens[elements] == NULL) goto cleanup;
    elements++;
    *count = elements;
    return tokens;

cleanup:
    {
        int i;
        for (i = 0; i < elements; i++) ni_string_ptr_free(tokens[i]);
        ni_string_ptr_free(tokens);
        *count = 0;
        return NULL;
    }
}

/* Free the result returned by ni_string_split_len(), or do nothing if 'tokens' is NULL. */
void ni_string_free_split_res(ni_string *tokens, int count) {
    if (!tokens) return;
    while (count--)
        ni_string_ptr_free(tokens[count]);
    ni_string_ptr_free(tokens);
}

/* Append to the ni_string "s" an escaped string representation where
 * all the non-printable characters (tested with isprint()) are turned into
 * escapes in the form "\n\r\a...." or "\x<hex-number>".
 *
 * After the call, the modified ni_string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
ni_string ni_string_cat_repr(ni_string s, const char *p, size_t len) {
    s = ni_string_cat_len(s, "\"", 1);
    while (len--) {
        switch (*p) {
        case '\\':
        case '"':
            s = ni_string_cat_printf(s, "\\%c", *p);
            break;
        case '\n': s = ni_string_cat_len(s, "\\n", 2); break;
        case '\r': s = ni_string_cat_len(s, "\\r", 2); break;
        case '\t': s = ni_string_cat_len(s, "\\t", 2); break;
        case '\a': s = ni_string_cat_len(s, "\\a", 2); break;
        case '\b': s = ni_string_cat_len(s, "\\b", 2); break;
        default:
            if (isprint(*p))
                s = ni_string_cat_printf(s, "%c", *p);
            else
                s = ni_string_cat_printf(s, "\\x%02x", (unsigned char)*p);
            break;
        }
        p++;
    }
    return ni_string_cat_len(s, "\"", 1);
}

/* Helper function for ni_string_split_args() that returns non zero if 'c'
 * is a valid hex digit. */
int is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/* Helper function for ni_string_split_args() that converts a hex digit into an
 * integer from 0 to 15 */
int hex_digit_to_int(char c) {
    switch (c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': case 'A': return 10;
    case 'b': case 'B': return 11;
    case 'c': case 'C': return 12;
    case 'd': case 'D': return 13;
    case 'e': case 'E': return 14;
    case 'f': case 'F': return 15;
    default: return 0;
    }
}

/* Split a line into arguments, where every argument can be in the
 * following programming-language REPL-alike form:
 *
 * foo bar "newline are supported\n" and "\xff\x00otherstuff"
 *
 * The number of arguments is stored into *argc, and an array
 * of ni_string is returned.
 *
 * The caller should free the resulting array of ni_strings with
 * ni_string_free_split_res().
 *
 * Note that ni_string_cat_repr() is able to convert back a string into
 * a quoted string in the same format ni_string_split_args() is able to parse.
 *
 * The function returns the allocated tokens on success, even when the
 * input string is empty, or NULL if the input contains unbalanced
 * quotes or closed quotes followed by non space characters
 * as in: "foo"bar or "foo'
 */
ni_string *ni_string_split_args(const char *line, int *argc) {
    const char *p = line;
    char *current = NULL;
    char **vector = NULL;

    *argc = 0;
    while (1) {
        /* skip blanks */
        while (*p && isspace(*p)) p++;
        if (*p) {
            /* get a token */
            int inq = 0;  /* set to 1 if we are in "quotes" */
            int insq = 0; /* set to 1 if we are in 'single quotes' */
            int done = 0;

            if (current == NULL) current = ni_string_empty();
            while (!done) {
                if (inq) {
                    if (*p == '\\' && *(p + 1) == 'x' &&
                                             is_hex_digit(*(p + 2)) &&
                                             is_hex_digit(*(p + 3))) {
                        unsigned char byte;
                        byte = (hex_digit_to_int(*(p + 2)) * 16) +
                                hex_digit_to_int(*(p + 3));
                        current = ni_string_cat_len(current, (char*)&byte, 1);
                        p += 3;
                    } else if (*p == '\\' && *(p + 1)) {
                        char c;
                        p++;
                        switch (*p) {
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                        case 't': c = '\t'; break;
                        case 'b': c = '\b'; break;
                        case 'a': c = '\a'; break;
                        default: c = *p; break;
                        }
                        current = ni_string_cat_len(current, &c, 1);
                    } else if (*p == '"') {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (*(p + 1) && !isspace(*(p + 1))) goto err;
                        done = 1;
                    } else if (!*p) {
                        /* unterminated quotes */
                        goto err;
                    } else {
                        current = ni_string_cat_len(current, p, 1);
                    }
                } else if (insq) {
                    if (*p == '\\' && *(p + 1) == '\'') {
                        p++;
                        current = ni_string_cat_len(current, "'", 1);
                    } else if (*p == '\'') {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (*(p + 1) && !isspace(*(p + 1))) goto err;
                        done = 1;
                    } else if (!*p) {
                        /* unterminated quotes */
                        goto err;
                    } else {
                        current = ni_string_cat_len(current, p, 1);
                    }
                } else {
                    switch (*p) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                    case '\0':
                        done = 1;
                        break;
                    case '"':
                        inq = 1;
                        break;
                    case '\'':
                        insq = 1;
                        break;
                    default:
                        current = ni_string_cat_len(current, p, 1);
                        break;
                    }
                }
                if (*p) p++;
            }
            /* add the token to the vector */
            vector = ni_string_realloc(vector, ((*argc) + 1) * sizeof(char*));
            vector[*argc] = current;
            (*argc)++;
            current = NULL;
        } else {
            /* Even on empty input string return something not NULL. */
            if (vector == NULL) vector = ni_string_malloc(sizeof(void*));
            return vector;
        }
    }

err:
    while ((*argc)--)
        ni_string_ptr_free(vector[*argc]);
    ni_string_ptr_free(vector);
    if (current) ni_string_ptr_free(current);
    *argc = 0;
    return NULL;
}

/* Modify the string substituting all the occurrences of the set of
 * characters specified in the 'from' string to the corresponding character
 * in the 'to' array.
 *
 * For instance: ni_string_map_chars(mystring, "ho", "01", 2)
 * will have the effect of turning the string "hello" into "0ell1".
 *
 * The function returns the ni_string pointer, that is always the same
 * as the input pointer since no resize is needed. */
ni_string ni_string_map_chars(ni_string s, const char *from, const char *to, size_t setlen) {
    size_t j, i, l = ni_string_len(s);

    for (j = 0; j < l; j++) {
        for (i = 0; i < setlen; i++) {
            if (s[j] == from[i]) {
                s[j] = to[i];
                break;
            }
        }
    }
    return s;
}

/* Join an array of C strings using the specified separator (also a C string).
 * Returns the result as an ni_string. */
ni_string ni_string_join(char **argv, int argc, char *sep) {
    ni_string join = ni_string_empty();
    int j;

    for (j = 0; j < argc; j++) {
        join = ni_string_cat(join, argv[j]);
        if (j != argc - 1) join = ni_string_cat(join, sep);
    }
    return join;
}

/* Like ni_string_join, but joins an array of ni_strings. */
ni_string ni_string_join_ni_string(ni_string *argv, int argc, const char *sep, size_t seplen) {
    ni_string join = ni_string_empty();
    int j;

    for (j = 0; j < argc; j++) {
        join = ni_string_cat_ni_string(join, argv[j]);
        if (j != argc - 1) join = ni_string_cat_len(join, sep, seplen);
    }
    return join;
}

/* Wrappers to the allocators used by ni_string. Note that ni_string will actually
 * just use the macros defined into ni_string_alloc.h in order to avoid to pay
 * the overhead of function calls. Here we define these wrappers only for
 * the programs ni_string is linked to, if they want to touch the ni_string internals
 * even if they use a different allocator. */
void *ni_string_malloc(size_t size) { return ni_malloc(size); }
void *ni_string_realloc(void *ptr, size_t size) { return ni_realloc(ptr, size); }
void ni_string_ptr_free(void *ptr) { ni_free(ptr); }
