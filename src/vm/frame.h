#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "vm/page.h"
#include "threads/thread.h"

struct hash frame_table;

struct fte {
    struct hash_elem elem;
    int frame_number; // key

    tid_t tid;
    struct s_page *p;
    int bit_reference;
};

void frame_init();
void frame_free();

struct fte *frame_lookup();

void frame_allocate();
void frame_deallocate();
void frame_evict();

unsigned frame_hash (const struct hash_elem *h, void *aux);
bool frame_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);
#endif