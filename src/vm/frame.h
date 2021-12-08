#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "vm/page.h"
#include "threads/thread.h"
#include "threads/palloc.h"

struct hash *frame_table; // for global frame_table
struct list *frame_list; // for clock algorithm

struct fte {
    struct hash_elem helem;
    struct list_elem lelem;
    uint8_t *frame_number; // key
    
    void *kpage;
    void *upage;

    struct thread *t;
    struct s_pte *s_pte;
    bool bit_not_evict; //don't ban with this flag on
    int bit_reference;
};

unsigned frame_hash (const struct hash_elem *h, void *aux);
bool frame_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);
void frame_init();

void frame_destroy (struct hash_elem *e, void *aux);
void frame_free(struct thread *t);

struct fte *frame_lookup(uint8_t *frame_number);

void *frame_allocate(void *upage, enum palloc_flags flag);
void frame_deallocate();
void frame_evict();

#endif