#include <stdio.h>
#include "ni_test.h"

#define UNUSED(x) ((void)(x))

int ni_malloc_test(int argc, char **argv) {
    void *ptr;
    UNUSED(argc);
    UNUSED(argv);
    printf("Initial used memory: %zu\n", ni_malloc_used_memory());
    ptr = ni_malloc(123);
    printf("Allocated 123 bytes; used: %zu\n", ni_malloc_used_memory());
    ptr = ni_realloc(ptr, 456);
    printf("Reallocated to 456 bytes; used: %zu\n", ni_malloc_used_memory());
    ni_free(ptr);
    printf("Freed pointer; used: %zu\n", ni_malloc_used_memory());
    return 0;
}
