/* ni_malloc.c - total amount of allocated memory aware version of malloc()
 *
 * Copyright (c) 2020-2030, Lei Wang <wanglei_gmgc@163.com>
 * All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include "ni_malloc.h"
#include "ni_atomic.h"

#ifdef HAVE_MALLOC_SIZE
#define PREFIX_SIZE (0)
#else
#if defined(__sun) || defined(__sparc) || defined(__sparc__)
#define PREFIX_SIZE (sizeof(long long))
#else
#define PREFIX_SIZE (sizeof(size_t))
#endif
#endif

#define update_ni_malloc_stat_alloc(__n) do { \
    size_t _n = (__n); \
    if (_n&(sizeof(long)-1)) _n += sizeof(long)-(_n&(sizeof(long)-1)); \
    atomicIncr(used_memory, __n); \
} while(0)

#define update_ni_malloc_stat_free(__n) do { \
    size_t _n = (__n); \
    if (_n&(sizeof(long)-1)) _n += sizeof(long)-(_n&(sizeof(long)-1)); \
    atomicDecr(used_memory, __n); \
} while(0)

static size_t used_memory = 0;
pthread_mutex_t used_memory_mutex = PTHREAD_MUTEX_INITIALIZER;

static void ni_malloc_default_oom(size_t size) {
    fprintf(stderr, "ni_malloc: Out of memory trying to allocate %zu bytes.\n", size);
    fflush(stderr);
    abort();
}

static void (*ni_malloc_oom_handler)(size_t) = ni_malloc_default_oom;

void *ni_malloc(size_t size) {
    void *ptr = malloc(size + PREFIX_SIZE);
    if (!ptr)
        ni_malloc_oom_handler(size);
#ifdef HAVE_MALLOC_SIZE
    update_ni_malloc_stat_alloc(ni_malloc_size(ptr));
    return ptr;
#else
    *((size_t*)ptr) = size;
    update_ni_malloc_stat_alloc(size + PREFIX_SIZE);
    return (char*)(ptr + PREFIX_SIZE);
#endif
}

void *ni_calloc(size_t mblock, size_t size) {
    void *ptr = calloc(mblock, size + PREFIX_SIZE);
    if (!ptr)
        ni_malloc_oom_handler(size);
#ifdef HAVE_MALLOC_SIZE
    update_ni_malloc_stat_alloc(ni_malloc_size(ptr));
    return ptr;
#else
    *((size_t*)ptr) = size;
    update_ni_malloc_stat_alloc(size + PREFIX_SIZE);
    return (char*)(ptr + PREFIX_SIZE);
#endif
}

void *ni_realloc(void *ptr, size_t size) {
#ifndef HAVE_MALLOC_SIZE
    void *realptr;
#endif
    size_t oldsize;
    void *newptr;

    if (ptr == NULL)
        return ni_malloc(size);
#ifdef HAVE_MALLOC_SIZE
    oldsize = ni_malloc_size(ptr);
    newptr = realloc(ptr, size);
    if (!newptr)
        ni_malloc_oom_handler(size);
    update_ni_malloc_stat_free(oldsize);
    update_ni_malloc_stat_alloc(ni_malloc_size(newptr));
    return newptr;
#else
    realptr = (char*)(ptr - PREFIX_SIZE);
    oldsize = *((size_t*)realptr);
    newptr = realloc(realptr, size + PREFIX_SIZE);
    if (!newptr)
        ni_malloc_oom_handler(size);
    *((size_t*)newptr) = size;
    update_ni_malloc_stat_free(oldsize + PREFIX_SIZE);
    update_ni_malloc_stat_alloc(size + PREFIX_SIZE);
    return (char*)(newptr + PREFIX_SIZE);
#endif
}

/* Provide ni_malloc_size() for systems where this function is not provided by
 * malloc itself, given that in that case we store a header with this
 * information as the first bytes of every allocation. */
#ifndef HAVE_MALLOC_SIZE
size_t ni_malloc_size(void *ptr) {
    void *realptr = (char*)(ptr - PREFIX_SIZE);
    size_t size = *((size_t*)realptr);
    /* Assume at least that all the allocations are padded at sizeof(long) by
     * the underlying allocator. */
    if (size & (sizeof(long)-1))
        size += sizeof(long) - (size&(sizeof(long)-1));
    return size + PREFIX_SIZE;
}

size_t ni_malloc_usable(void *ptr) {
    return ni_malloc_size(ptr) - PREFIX_SIZE;
}
#endif

void ni_free(void *ptr) {
#ifndef HAVE_MALLOC_SIZE
    void *realptr;
    size_t oldsize;
#endif
    if (ptr == NULL)
        return;
#ifdef HAVE_MALLOC_SIZE
    update_ni_malloc_stat_free(ni_malloc_size(ptr));
    free(ptr);
#else
    realptr = (char*)(ptr - PREFIX_SIZE);
    oldsize = *((size_t*)realptr);
    update_ni_malloc_stat_free(oldsize + PREFIX_SIZE);
    free(realptr);
#endif
}

size_t ni_malloc_used_memory(void) {
    size_t um;
    atomicGet(used_memory, um);
    return um;
}

void ni_malloc_set_oom_handler(void (*oom_handler)(size_t)) {
    ni_malloc_oom_handler = oom_handler;
}

/* Get the RSS information in an OS-specific way.
 *
 * WARNING: the function zmalloc_get_rss() is not designed to be fast
 * and may not be called in the busy loops where Redis tries to release
 * memory expiring or swapping out objects.
 *
 * For this kind of "fast RSS reporting" usages use instead the
 * function RedisEstimateRSS() that is a much faster (and less precise)
 * version of the function. */

#if defined(HAVE_PROC_STAT)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

size_t ni_malloc_get_rss(void) {
    int page = sysconf(_SC_PAGESIZE);
    size_t      rss;
    char        buf[4096];
    char        filename[256];
    int         fd, count;
    char        *p, *x;

    snprintf(filename, 256, "/proc/%d/stat", getpid());
    if ((fd = open(filename,O_RDONLY)) == -1)
        return 0;
    if (read(fd,buf,4096) <= 0) {
        close(fd);
        return 0;
    }
    close(fd);

    p = buf;
    count = 23; /* RSS is the 24th field in /proc/<pid>/stat */
    while(p && count--) {
        p = strchr(p,' ');
        if (p)
            p++;
    }
    if (!p) return 0;
    x = strchr(p,' ');
    if (!x) return 0;
    *x = '\0';

    rss = strtoll(p, NULL, 10);
    rss *= page;
    return rss;
}

#elif defined(HAVE_TASKINFO)
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/task.h>
#include <mach/mach_init.h>

size_t ni_malloc_get_rss(void) {
    task_t task = MACH_PORT_NULL;
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (task_for_pid(current_task(), getpid(), &task) != KERN_SUCCESS)
        return 0;
    task_info(task, TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);

    return t_info.resident_size;
}
#else
size_t ni_malloc_get_rss(void) {
    /* If we can't get the RSS in an OS-specific way for this system just
     * return the memory usage we estimated in ni_malloc()..
     *
     * Fragmentation will appear to be always 1 (no fragmentation)
     * of course... */
    return ni_malloc_used_memory();
}
#endif

int ni_malloc_get_allocator_info(size_t *allocated,
                                    size_t *active,
                                    size_t *resident) {
    *allocated = *resident = *active = 0;
    return 1;
}

/* Get the sum of the specified field (converted form kb to bytes) in
 * /proc/self/smaps. The field must be specified with trailing ":" as it
 * apperas in the smaps output.
 *
 * If a pid is specified, the information is extracted for such a pid,
 * otherwise if pid is -1 the information is reported is about the
 * current process.
 *
 * Example: zmalloc_get_smap_bytes_by_field("Rss:",-1);
 */
#if defined(HAVE_PROC_SMAPS)
size_t ni_malloc_get_smap_bytes_by_field(char *field, long pid) {
    char line[1024];
    size_t bytes = 0;
    int flen = strlen(field);
    FILE *fp;

    if (pid == -1)
        fp = fopen("/proc/self/smaps","r");
    else {
        char filename[128];
        snprintf(filename,sizeof(filename),"/proc/%ld/smaps",pid);
        fp = fopen(filename,"r");
    }

    if (!fp) return 0;
    while(fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, field, flen) == 0) {
            char *p = strchr(line, 'k');
            if (p) {
                *p = '\0';
                bytes += strtol(line + flen, NULL, 10) * 1024;
            }
        }
    }
    fclose(fp);
    return bytes;
}
#else
size_t ni_malloc_get_smap_bytes_by_field(char *field, long pid) {
    ((void) field);
    ((void) pid);
    return 0;
}
#endif

/* Returns the size of physical memory (RAM) in bytes.
 * It looks ugly, but this is the cleanest way to achieve cross platform results.
 * Cleaned up from:
 *
 * http://nadeausoftware.com/articles/2012/09/c_c_tip_how_get_physical_memory_size_system
 *
 * Note that this function:
 * 1) Was released under the following CC attribution license:
 *    http://creativecommons.org/licenses/by/3.0/deed.en_US.
 * 2) Was originally implemented by David Robert Nadeau.
 * 3) Was modified for Redis by Matt Stancliff.
 * 4) This note exists in order to comply with the original license.
 */
size_t ni_malloc_get_memory_size(void) {
#if defined(__unix__) || defined(__unix) || defined(unix) || \
    (defined(__APPLE__) && defined(__MACH__))
#if defined(CTL_HW) && (defined(HW_MEMSIZE) || defined(HW_PHYSMEM64))
    int mib[2];
    mib[0] = CTL_HW;
#if defined(HW_MEMSIZE)
    mib[1] = HW_MEMSIZE;            /* OSX. --------------------- */
#elif defined(HW_PHYSMEM64)
    mib[1] = HW_PHYSMEM64;          /* NetBSD, OpenBSD. --------- */
#endif
    int64_t size = 0;               /* 64-bit */
    size_t len = sizeof(size);
    if (sysctl( mib, 2, &size, &len, NULL, 0) == 0)
        return (size_t)size;
    return 0L;          /* Failed? */

#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
    /* FreeBSD, Linux, OpenBSD, and Solaris. -------------------- */
    return (size_t)sysconf(_SC_PHYS_PAGES) * (size_t)sysconf(_SC_PAGESIZE);

#elif defined(CTL_HW) && (defined(HW_PHYSMEM) || defined(HW_REALMEM))
    /* DragonFly BSD, FreeBSD, NetBSD, OpenBSD, and OSX. -------- */
    int mib[2];
    mib[0] = CTL_HW;
#if defined(HW_REALMEM)
    mib[1] = HW_REALMEM;        /* FreeBSD. ----------------- */
#elif defined(HW_PHYSMEM)
    mib[1] = HW_PHYSMEM;        /* Others. ------------------ */
#endif
    unsigned int size = 0;      /* 32-bit */
    size_t len = sizeof(size);
    if (sysctl(mib, 2, &size, &len, NULL, 0) == 0)
        return (size_t)size;
    return 0L;          /* Failed? */
#else
    return 0L;          /* Unknown method to get the data. */
#endif
#else
    return 0L;          /* Unknown OS. */
#endif
}
