#include <iostream>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>

const size_t BLOCK_SIZE = 128; // Minimum block size
const size_t MAX_ORDER = 10; // 10 represents the largest block (128 KB) 
const size_t MEMORY_POOL_SIZE = 32 * (128 * 1024); // Total size for 32 blocks of 128KB

size_t _num_allocated_blocks();

struct Metadata {
    size_t size;
    bool is_free;
    Metadata* next;
    Metadata* prev;
    Metadata* buddy;
    int order;
};
class LinkedList {
public:
    Metadata* ListHead;

    LinkedList() : ListHead(NULL) {}

    void enqueueToList(Metadata* m) {
        if (ListHead == NULL) { // if list is empty
            ListHead = m;
            m->next = nullptr;
            m->prev = nullptr;
            return;
        }

        Metadata* curr = ListHead;

        while (curr->next != NULL && (char*)curr < (char*)m) {
            curr = curr->next;
        }
        if (curr == ListHead) { // Insert at the beginning
            m->next = ListHead;
            m->prev = nullptr;
            ListHead->prev = m;
            ListHead = m;
        }
        else if (curr == nullptr) { // Insert at the end
            Metadata* last = ListHead;
            while (last->next != nullptr) {
                last = last->next;
            }
            last->next = m;
            m->prev = last;
            m->next = nullptr;
        }
        else { // Insert in the middle
            m->prev = curr->prev;
            m->next = curr;
            curr->prev->next = m;
            curr->prev = m;
        }
    }
    void removeFromList(Metadata* m) {
        if (m->prev == nullptr && m->next == nullptr) { // last one case
            ListHead = nullptr;
            return;
        }
        if (m->prev != nullptr) {
            m->prev->next = m->next;
        }
        else {
            ListHead = m->next; // Head case
            //std::cout << "New Head Size: " << ListHead->size << std::endl;
        }
        if (m->next != nullptr) {
            m->next->prev = m->prev;
        }
        m->next = nullptr;
        m->prev = nullptr;
    }


    Metadata* getMetadata(void* block) {
        return (Metadata*)((char*)block - sizeof(Metadata));
    }

};
class BuddyAllocator {

public:
    LinkedList free_lists[MAX_ORDER + 1]; // Array of pointers for free lists
    Metadata* blocks;
    int big_counter;
    int big_bytes;
    bool init;
    BuddyAllocator() {
        void* allocated_memory = sbrk(MEMORY_POOL_SIZE);
        if (allocated_memory == (void*)-1) {
            std::cerr << "Failed to allocate memory" << std::endl;
            exit(EXIT_FAILURE);
        }

        blocks = (Metadata*)allocated_memory;
        init = false;
        big_counter = 0;
        big_bytes = 0;

    }
    void run_init() {
        //std::cout << "SIZE OF METADATA" << sizeof(Metadata) << std::endl;
        // Initialize 32 blocks of 128 KB each
        for (size_t i = 0; i < 32; ++i) {
            Metadata* block = (Metadata*)((char*)blocks + i * (128 * 1024));
            block->size = (128 * 1024) - sizeof(Metadata); // Set size excluding metadata
            block->is_free = true;
            block->next = nullptr;
            block->prev = nullptr;
            block->buddy = nullptr;
            block->order = MAX_ORDER;

            // Enqueue the new free block into the free list for the maximum order
            free_lists[MAX_ORDER].enqueueToList(block);
        }
        init = true;
    }

    // Function to find the smallest free block capable of holding the requested size
    void* allocate(size_t size) {
        if (size >= 128 * 1024) { // For large allocations
            void* ptr = mmap(nullptr, size + sizeof(Metadata),
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS,
                -1, 0);
            if (ptr == MAP_FAILED) {
                return nullptr;
            }
            Metadata* metadata = (Metadata*)ptr;
            metadata->size = size;
            metadata->is_free = false;
            metadata->buddy = nullptr;
            big_counter++;
            big_bytes += size;
            return (void*)((char*)ptr + sizeof(Metadata));
        }
        else {
            int order = calculateOrder(size);
            for (int i = order; i <= int(MAX_ORDER); ++i) {
                if (free_lists[i].ListHead != nullptr) {
                    Metadata* block = free_lists[i].ListHead;
                    while (block->is_free == false && block->next != nullptr) {
                        block = block->next;
                    }
                    if (block->is_free) {
                        //free_lists[i].removeFromList(block);
                        // Split the block if necessary
                        free_lists[i].removeFromList(block);

                        while (i > order) {
                            i--;
                            block = splitBlock(block);
                        }

                        free_lists[i].enqueueToList(block);
                        block->is_free = false; // Mark as allocated
                        return (void*)((char*)block + sizeof(Metadata));
                    }

                }
            }
        }
        return nullptr; // No suitable block found
    }

    // Function to calculate the order based on size
    int calculateOrder(size_t size) {
        size += sizeof(Metadata); // Include metadata size in the allocation request
        int order = 0;
        while ((1U << (order + 7)) < size) {
            order++;
        }
        return order;
    }

    // Function to split a block for buddy allocation
    Metadata* splitBlock(Metadata* block) {
        size_t oldSize = block->size + sizeof(Metadata);
        // Create buddy block
        Metadata* buddy = (Metadata*)((char*)block + oldSize / 2);
        buddy->size = oldSize / 2 - sizeof(Metadata);
        buddy->is_free = true;
        buddy->next = nullptr;
        buddy->prev = nullptr;
        buddy->order = block->order - 1;
        if (block->buddy != nullptr) {
            buddy->buddy = block->buddy;
        }
        else {
            buddy->buddy = block;
        }

        //free_lists[block->order].removeFromList(block);

        // Update the original block
        block->size = oldSize / 2 - sizeof(Metadata);
        block->order--;
        block->buddy = buddy;

        // Add buddy to the appropriate free list
        free_lists[buddy->order].enqueueToList(buddy);
        return block; // Return the original block, which was split
    }

    // Function to free a block and merge it with its buddy if possible
    void free(void* ptr) {
        if (!ptr) return;
        Metadata* block = (Metadata*)((char*)ptr - sizeof(Metadata));
        if (block->size >= 128 * 1024) {
            int size = block->size;
            if (munmap(block, block->size + sizeof(Metadata)) == -1) {
                std::cerr << "Failed to unmap memory" << std::endl;
                return;
            }
            big_counter--;
            big_bytes = big_bytes - size;
            return;
        }
        else {
            block->is_free = true;
            if (block->buddy) {
                Metadata* prev_buddy = block - (block->buddy) + block;
                if (prev_buddy->buddy == block && prev_buddy->is_free) {
                    mergeWithBuddy(prev_buddy);
                }
            }
            mergeWithBuddy(block);
        }
    }

    // Function to merge a block with its buddy
    void mergeWithBuddy(Metadata* block) {
        int order = block->order;
        //size_t buddyAddress = (size_t)block ^ (1 << (order + 7)); // Calculate buddy's address
        //size_t buddyAddress = (size_t)block->buddy;
        //Metadata* buddy = reinterpret_cast<Metadata*>(buddyAddress);
        if (block->buddy != nullptr) {
            Metadata* buddy = block->buddy;
            if (buddy &&
                buddy->size < ((128 * 1024) - sizeof(Metadata)) &&
                buddy->size == block->size &&
                buddy >= blocks &&
                buddy < (Metadata*)((char*)blocks + MEMORY_POOL_SIZE) &&
                buddy->is_free &&
                buddy->order == order) {
                // Remove buddy from the free list
                free_lists[order].removeFromList(buddy);
                free_lists[order].removeFromList(block);
                // Merge blocks
                
                if (buddy < block) {
                    std::swap(block, buddy); // Ensure block points to lower address
                }
                
                // Update block size and order
                block->size += buddy->size + sizeof(Metadata);
                block->order++;
                if (buddy->buddy != nullptr && buddy->buddy != block) {
                    block->buddy = buddy->buddy;
                }
                else {
                    block->buddy = nullptr;
                }
                // Add the merged block back to the free list
                free_lists[block->order].enqueueToList(block);
                mergeWithBuddy(block);
            }
        }
        }
        // Check if buddy is within valid range and free
};


//LinkedList blocks = LinkedList();
BuddyAllocator buddy_alc;

void* smalloc(size_t size) {
    if (buddy_alc.init== false) {
        buddy_alc.run_init();
    }


    if (size == 0 || size > 100000000) {
        return NULL;
    }

    void* block = buddy_alc.allocate(size);
    if (block == NULL) {
        return NULL;
    }
    return (char*)block;
}

void* scalloc(size_t num, size_t size) {
    if (buddy_alc.init == false) {
        buddy_alc.run_init();
    }


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
        buddy_alc.free(p);
    }
}

void* srealloc(void* oldp, size_t size) {
    if (buddy_alc.init == false) {
        buddy_alc.run_init();
    }

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

    Metadata* metadata = (Metadata*)((char*)oldp - sizeof(Metadata));
    size_t old_size = metadata->size + sizeof(Metadata); // Include metadata size

    // If the current block is large enough, reuse it directly
    if (old_size >= size + sizeof(Metadata)) {
        return oldp; // Return the existing pointer
    }
    /**
    // Attempt to merge with buddies
    int order = metadata->order;
    size_t block_size = old_size;
    // Attempt merging with buddy blocks until we have enough space
    while (order < int(MAX_ORDER)) {
        size_t buddyAddress = (size_t)metadata ^ (1 << (order + 7)); // Calculate buddy's address
        Metadata* buddy = reinterpret_cast<Metadata*>(buddyAddress);

        // Check if the buddy is valid and free
        if (buddy >= buddy_alc.blocks &&
            buddy < (Metadata*)((char*)buddy_alc.blocks + MEMORY_POOL_SIZE) &&
            buddy->is_free &&
            buddy->order == order) {
            // Merge the buddy block into the current block
            buddy_alc.free_lists[order].removeFromList(buddy); // Remove buddy from free list
            if (buddyAddress < (size_t)metadata) {
                std::swap(metadata, buddy); // Ensure metadata is the lower address
            }

            // Merge blocks
            metadata->size += buddy->size + sizeof(Metadata);
            metadata->order++;
            block_size = metadata->size + sizeof(Metadata);

            // If merged block is large enough now, return it
            if (block_size >= size + sizeof(Metadata)) {
                metadata->is_free = false; // Mark as allocated
                return (void*)((char*)metadata + sizeof(Metadata)); // Return usable block
            }
        }
        else {
            break; // Buddy not available to merge, exit loop
        }
    }

    */
    // If we cannot accommodate the request by merging, find a new block
    metadata->is_free = true;
    void* new_block = buddy_alc.allocate(size);
    //_num_allocated_blocks();
    if (new_block != NULL) {
        // Copy the old data to the new block
        //size_t bytes_to_copy = (old_size < size) ? old_size : size;
        memcpy(new_block, oldp, old_size); // Copy data
        sfree(oldp); // Free the old block
    }
    if (new_block == NULL) {
        metadata->is_free = false;
    }

    return new_block; // Return the new block or NULL if allocation failed
}

size_t _num_free_blocks() {
    size_t free_blocks = 0;
    for (int i = 0; i <= int(MAX_ORDER); i++) {
        Metadata* curr = buddy_alc.free_lists[i].ListHead;
        while (curr != nullptr) {
            if (curr->is_free) {
                free_blocks++;
            }
            curr = curr->next;
        }
    }
    return free_blocks;
}

size_t _num_free_bytes() {
    size_t free_bytes = 0;
    for (int i = 0; i <= int(MAX_ORDER); i++) {
        Metadata* curr = buddy_alc.free_lists[i].ListHead;
        while (curr != nullptr) {
            if (curr->is_free) {
                free_bytes += curr->size; // Accumulate sizes of free blocks
            }
            curr = curr->next;
        }
    }
    return free_bytes;
}

size_t _num_allocated_blocks() {
    size_t allocated_blocks = 0;
    for (int i = 0; i <= int(MAX_ORDER); i++) {
        Metadata* curr = buddy_alc.free_lists[i].ListHead;
        while (curr != nullptr) {
            if (curr->is_free == false) {
            }
            allocated_blocks++;
            curr = curr->next;
        }
    }
    return allocated_blocks + buddy_alc.big_counter;
}

size_t _num_meta_data_bytes() {
    size_t meta_count = 0;
    for (int i = 0; i <= int(MAX_ORDER); i++) {
        Metadata* curr = buddy_alc.free_lists[i].ListHead;
        while (curr != nullptr) {
            meta_count++;
            curr = curr->next;
        }
    }
    return (meta_count + buddy_alc.big_counter) * sizeof(Metadata); // Total metadata bytes for all blocks
}

size_t _num_allocated_bytes() {
    size_t allocated_bytes = 0;
    for (int i = 0; i <= int(MAX_ORDER); i++) {
        Metadata* curr = buddy_alc.free_lists[i].ListHead;
        while (curr != nullptr) {
            allocated_bytes+= curr->size;
            curr = curr->next;
        }
    }
    // Each block that is free will account for a total of the initialized blocks
    //return buddy_alc.allocated_blocks + _num_free_blocks(); // Returning total counted blocks
    return allocated_bytes + buddy_alc.big_bytes;
}


size_t _size_meta_data() {
    return sizeof(Metadata);
}