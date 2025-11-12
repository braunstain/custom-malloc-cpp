#include <stdio.h>
#include <unistd.h>

void* smalloc(size_t size) {
    if (size == 0 || size > 100000000) {
        return NULL;
    }

    void* ptr = sbrk(size);
    if (*(int*)ptr == -1) {
        return NULL;
    }
    return ptr;
}
