#include <iostream>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

struct Metadata {
    size_t size;
    bool is_free;
    Metadata* next;
    Metadata* prev;
};

class LinkedList {
  public:
    Metadata* ListHead;

    LinkedList(): ListHead(NULL) {}

    void enqueueToList(Metadata* m) {
        if (ListHead == NULL) { // if list is empty
            ListHead = m;
            return;
        }

        Metadata* curr = ListHead;

        while (curr->next != NULL) {
            curr = curr->next;
        }

        curr->next = m;
        m->prev = curr;
    }

    void* allocateBlock(size_t size) {
        Metadata* curr = ListHead;

        while (curr != NULL) {
            if (curr->size >= size && curr->is_free == true) {
                curr->is_free = false;
                return curr;
            }
            curr = curr->next;
        }

        void* ptr = sbrk(size+sizeof(Metadata));
        if (ptr == (void*)-1) {
            return NULL;
        }

        Metadata* newMetaData = (Metadata*)ptr;
        newMetaData->size = size;
        newMetaData->is_free = false;
        newMetaData->prev = NULL;
        newMetaData->next = NULL;
        enqueueToList(newMetaData);
        return ptr;
    }

    Metadata* getMetadata(void* block) {
        return (Metadata*)((char*)block - sizeof(Metadata));
    }

    void freeBlock(void* block) {
        Metadata* metadata = getMetadata(block);
        metadata->is_free = true;
    }
};


LinkedList blocks = LinkedList();

void* smalloc(size_t size) {

    if (size == 0 || size > 100000000) {
        return NULL;
    }

    void* block = blocks.allocateBlock(size);
    if (block == NULL) {
        return NULL;
    }
    return (char*)block + sizeof(Metadata);
}

void* scalloc(size_t num, size_t size) {
    size = size * num;
    void* block = smalloc(size);
    if (block == NULL) {
        return NULL;
    }
    memset(block, 0, size);
    return block;
}

void sfree(void* p) {
    if (p != NULL) {
        blocks.freeBlock(p);
    }
}

void* srealloc(void* oldp, size_t size) {
    if (size == 0 || size > 100000000) {
        return NULL;
    }

    if (oldp == NULL) {
        void* block = smalloc(size);
        if (block == NULL) {
            return NULL;
        }
        return block;
    }

    Metadata* metadata = blocks.getMetadata(oldp);
    size_t old_size = metadata->size;

    if (old_size >= size) {
        return oldp;
    }

    void* block = smalloc(size);
    if (block == NULL) {
        return NULL;
    }
    memmove(block, oldp, old_size);
    sfree(oldp);
    return block;
}

size_t _num_free_blocks() {
    size_t free_blocks = 0;
    Metadata* curr = blocks.ListHead;

    while (curr != NULL) {
        if (curr->is_free == true) {
            free_blocks++;
        }
        curr = curr->next;
    }
    return free_blocks;
}

size_t _num_free_bytes() {
    size_t free_bytes = 0;
    Metadata* curr = blocks.ListHead;

    while (curr != NULL) {
        if (curr->is_free == true) {
            free_bytes += curr->size;
        }
        curr = curr->next;
    }
    return free_bytes;
}

size_t _num_allocated_blocks() {
    size_t num_of_blocks = 0;
    Metadata* curr = blocks.ListHead;

    while (curr != NULL) {
        num_of_blocks++;
        curr = curr->next;
    }
    return num_of_blocks;
}

size_t _num_allocated_bytes() {
    size_t Bytes = 0;
    Metadata* curr = blocks.ListHead;

    while (curr != NULL) {
        Bytes += curr->size;
        curr = curr->next;
    }
    return Bytes;
}

size_t _size_meta_data() {
    return sizeof(Metadata);
}

size_t _num_meta_data_bytes() {
    return _num_allocated_blocks() * _size_meta_data();
}