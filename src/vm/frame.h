#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "vm/page.h"
#include "threads/thread.h"

struct hash frame_table;

struct fte {
    struct hash_elem elem;
    uint8_t *frame_number; // key

    tid_t tid;
    struct s_page *p;
    int bit_reference;
};

unsigned frame_hash (const struct hash_elem *h, void *aux);
bool frame_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);
void frame_init();

void frame_destroy (struct hash_elem *e, void *aux);
void frame_free(struct hash *target_table);

struct fte *frame_lookup(uint8_t *frame_number);

void frame_allocate();
void frame_deallocate();
void frame_evict();

#endif