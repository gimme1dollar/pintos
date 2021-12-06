#include "vm/page.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"

/* return key */
unsigned 
s_page_hash (const struct hash_elem *h, void *aux)
{
    struct s_pte *s_pt;
    
    s_pt = hash_entry(h, struct s_pte, elem);   

    return s_pt->table_number;
}

bool 
s_page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
    struct s_pte *s_pt_a, *s_pt_b;

    s_pt_a = hash_entry(a, struct s_pte, elem);   
    s_pt_b = hash_entry(b, struct s_pte, elem);   

    return s_pt_a->table_number < s_pt_b->table_number;
}

void 
s_page_init(struct hash *target_table)
{
  hash_init(target_table, s_page_hash, s_page_less, NULL);
}


void 
s_page_free(struct hash *target_table)
{
    return;
}


struct s_pte*
s_page_lookup(void *page)
{
    //printf("in s_page_lookup\n");
    struct thread *t;
    struct s_pte finder, *entry;
    struct hash_elem *target;

    t = thread_current();

    //printf("table number (page): %d\n", page);
    finder.table_number = page;
    //printf("table number: %d\n", finder.table_number);
    target = hash_find(t->s_page_table, &(finder.elem));
    //printf("target is null %d\n", target == NULL);

    if(target == NULL)
    {
        return NULL;
    }

    entry = hash_entry (target, struct s_pte, elem);
    
    if(entry == NULL)
    {
        return NULL;
    }

    return entry;
}


bool
s_page_load(struct s_pte *entry) 
{
    //printf("in s_page_load %d\n", entry->tid);
    switch (entry->type) 
    {
        case s_pte_type_STACK:
            return load_segment_stack(entry);
        case s_pte_type_FILE:
            return load_segment_from_file(entry);
        case s_pte_type_MMAP:
            return false;
        case s_pte_type_SWAP:
            return false;
        default:
            printf("something is wrong in s_pte type\n");
            return false;
    }
}

bool 
load_segment_from_file(struct s_pte *entry)
{
    bool page_install;

    /* Get a frame */
    void *frame = palloc_get_page (PAL_USER);
    if (frame == NULL)
        return false;

    /* Load this page. */
    file_seek (entry->file, entry->page_offset);
    if (file_read (entry->file, frame, entry->read_bytes) != (int) entry->read_bytes)
    {
        palloc_free_page (frame);
    //    printf("file load false\n");
        return false;
    }
    memset (frame + entry->read_bytes, 0, entry->zero_bytes);

    /* Link page and frame */
    page_install = pagedir_get_page (thread_current()->pagedir, entry->upage) == NULL
          && pagedir_set_page (thread_current()->pagedir, entry->upage, frame, entry->writable);
    if (!page_install)
    {
        palloc_free_page (frame);
   //     printf("file load false!\n");
        return false;
    }
 //   printf("file load true!\n");
    return true;
}

bool 
load_segment_from_mmap()
{
    return false;
}

bool 
load_segment_from_swap()
{
    return false;
}

bool 
load_segment_stack(struct s_pte *entry)
{
    bool page_install;

    /* get frame */
    uint8_t *frame;
    frame = palloc_get_page (PAL_USER | PAL_ZERO);
    if (frame == NULL)
        return false;
    
    /* set zero */
    memset (frame, 0, PGSIZE);

    /* Load this page. */
    page_install = pagedir_get_page (thread_current()->pagedir, entry->upage) == NULL
          && pagedir_set_page (thread_current()->pagedir, entry->upage, frame, entry->writable);
    if (!page_install)
    {
        palloc_free_page (frame);
        return false;
    }

    return page_install;
}

struct s_pte *
grow_stack(void* page)
{
    struct thread *t;
    struct s_pte* pte;
    size_t prevs;
    uint8_t start;

    t = thread_current ();

    /* Make a page table entry */
    pte = (struct s_pte *) malloc (sizeof(struct s_pte));
    if (pte == NULL)
        return false;

    pte->tid = t->tid;
    pte->type = s_pte_type_STACK;
    pte->table_number = page;
    
    pte->writable = true;
    pte->file;
    pte->upage = page;
    pte->file_page;
    pte->page_offset;
    pte->read_bytes;
    pte->zero_bytes;
    pte->mmap;
    pte->swap_slot;

    return pte;     
}