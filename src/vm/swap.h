#ifndef VM_SWAP_H
#define VM_SWAP_H
#include <bitmap.h>
#include "devices/block.h"
#include "threads/thread.h"

struct block *swap_disk; // swap disk
struct list *swap_table; // swap table
struct lock swap_lock;

struct ste {
    struct list_elem elem;

    uint32_t ste_id;
    bool is_free;
};

void swap_init (void);
void swap_destroy (uint32_t index);
void swap_free (void); // free all

void swap_in (uint32_t index, void *page);
uint32_t swap_out (void *page);

#endif