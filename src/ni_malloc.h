/* ni_malloc.h - total amount of allocated memory aware version of malloc()
 *
 * Copyright (c) 2020-2030, Lei Wang <wanglei_gmgc@163.com>
 * All rights reserved.
 *
 */

#ifndef _NI_MALLOC_H_
#define _NI_MALLOC_H_

#include <malloc.h>
#define HAVE_MALLOC_SIZE        1
#define ni_malloc_size(p)       malloc_usable_size(p)

void *ni_malloc(size_t size);
void *ni_calloc(size_t mblock, size_t size);
void *ni_realloc(void *ptr, size_t size);
void ni_free(void *ptr);
size_t ni_malloc_used_memory(void);
void ni_malloc_set_oom_handler(void (*oom_handler)(size_t));
size_t ni_malloc_get_rss(void);
int ni_malloc_get_allocator_info(size_t *allocated, size_t *active, size_t *resident);
size_t ni_malloc_get_smap_bytes_by_field(char *field, long pid);
size_t ni_malloc_get_memory_size(void);

#endif /* _NI_MALLOC_H_ */
