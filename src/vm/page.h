#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "threads/thread.h"
#include "filesys/off_t.h"

enum s_pte_type
{
    s_pte_type_STACK,
    s_pte_type_FILE,
    s_pte_type_SWAP,
    s_pte_type_MMAP
 };

struct s_pte {
    struct hash_elem elem;
    tid_t tid;
    int type; // { FILE, STACK, ... }
    int vaddr;

    /* to load from file */
    struct file *file;
    uint8_t *upage;
    uint32_t file_page;
    off_t page_offset;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;

    /* to load from mmap */
    size_t mmap; // ?

    /* to load from swap-slot */
    size_t swap_slot; // ?
};

void s_page_init();
void s_page_free();

struct s_pte *s_page_lookup();

void load_segment_from_file();
void load_segment_from_swap();
bool load_segment_stack(uint8_t *kpage);

unsigned s_page_hash (const struct hash_elem *h, void *aux);
bool s_page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);

#endif