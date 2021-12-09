#include "vm/page.h"
#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"

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
s_page_delete(struct hash *target_table, struct hash_elem *he)
{
  free(hash_delete (target_table, he));
}

void
s_page_destroy (struct hash_elem *e, void *aux)
{
    struct s_pte *s_pt;
    
    s_pt = hash_entry(e, struct s_pte, elem);  
    free(s_pt);

    return;
}

void 
s_page_free(struct hash *target_table)
{
    hash_destroy(target_table, s_page_destroy);
    
    return;
}


struct s_pte*
s_page_lookup(void *page)
{
    //printf("in s_page_lookup\n");
    struct thread *t;
    struct s_pte finder, *entry;
    struct hash_elem *target;

    /* find hash table with finder */
    t = thread_current();
    finder.table_number = page;
    target = hash_find(t->s_page_table, &(finder.elem));
    if(target == NULL)
    {
        return NULL;
    }

    /* find the entry from table */
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
    bool success; 

    /* load page with respect to its type */
    success = false;
    switch (entry->type) 
    {
        case s_pte_type_STACK:
            //printf("before laod stack!\n");
            success = load_segment_stack(entry);
            break;
        case s_pte_type_FILE:
            //printf("before laod file!\n");
            success = load_segment_from_file(entry);
            break;
        case s_pte_type_MMAP:
            //printf("before laod mmap!\n");
            success = load_segment_from_mmap(entry);
            break;
        case s_pte_type_SWAP:
            //printf("before laod swap!\n");
            success = load_segment_from_swap(entry);
            //printf("after laod swap!\n");
            break;
        default:
            printf("something is wrong in s_pte type\n");
            success = false;
            break;
    }
    if (!success) 
    {
        return false;
    }

    return success;
}

bool 
load_segment_from_file(struct s_pte *entry)
{
    bool page_install;

    /* Get a frame */
    void *frame = frame_allocate (entry->upage, PAL_USER);
    if (frame == NULL)
        return false;

    /* Set data to the frame from file */
    file_seek (entry->file, entry->page_offset);

    if(!lock_held_by_current_thread(&syscall_handler_lock))
        lock_acquire (&syscall_handler_lock);
    if (file_read (entry->file, frame, entry->read_bytes) != (int) entry->read_bytes)
    {
        frame_deallocate (frame, false);
        if(lock_held_by_current_thread(&syscall_handler_lock))
            lock_release (&syscall_handler_lock);
        return false;
    }
    if(lock_held_by_current_thread(&syscall_handler_lock))
        lock_release (&syscall_handler_lock);
    memset (frame + entry->read_bytes, 0, entry->zero_bytes);

    /* Link page and frame */
    page_install = pagedir_get_page (thread_current()->pagedir, entry->upage) == NULL
          && pagedir_set_page (thread_current()->pagedir, entry->upage, frame, entry->writable);
    if (!page_install)
    {
        frame_deallocate (frame, true);
        return false;
    }

    return true;
}

bool 
load_segment_from_mmap(struct s_pte *entry)
{
    bool page_install;

    /* Get a frame */
    void *frame = frame_allocate (entry->upage, PAL_USER);
    if (frame == NULL)
        return false;

    /* Set data to the frame from file */
    file_seek (entry->file, entry->page_offset);

    if(!lock_held_by_current_thread(&syscall_handler_lock))
        lock_acquire (&syscall_handler_lock);
    if (file_read (entry->file, frame, entry->read_bytes) != (int) entry->read_bytes)
    {
        frame_deallocate (frame, false);
        if(lock_held_by_current_thread(&syscall_handler_lock))
            lock_release (&syscall_handler_lock);
        return false;
    }
    if(lock_held_by_current_thread(&syscall_handler_lock))
        lock_release (&syscall_handler_lock);
    memset (frame + entry->read_bytes, 0, entry->zero_bytes);

    /* Link page and frame */
    page_install = pagedir_get_page (thread_current()->pagedir, entry->upage) == NULL
          && pagedir_set_page (thread_current()->pagedir, entry->upage, frame, entry->writable);
    if (!page_install)
    {
        frame_deallocate (frame, true);
        return false;
    }

    return true;
}

bool 
load_segment_from_swap(struct s_pte *entry)
{
    bool page_install;

    /* Get a frame */
    //printf("before frame allocate\n");
    void *frame = frame_allocate (entry->upage, PAL_USER);
    if (frame == NULL)
        return false;

    /* Load the data from swap_disk */
    //printf("before swap in\n");
    swap_in (entry->swap_slot, frame);
    entry->type = entry->prev_type;
    //printf("after swap in\n");

    /* Link page and frame */
    //printf("###### entry->upage: %#08X", entry->upage);
    page_install = pagedir_get_page (thread_current()->pagedir, entry->upage) == NULL
          && pagedir_set_page (thread_current()->pagedir, entry->upage, frame, entry->writable);
    if (!page_install)
    {
        printf("page install fail!\n");
        frame_deallocate (frame, true);
        return false;
    }

    return true;
}

bool 
load_segment_stack(struct s_pte *entry)
{
    bool page_install;

    /* get frame */
    uint8_t *frame;
    frame = frame_allocate (entry->upage, PAL_USER | PAL_ZERO);
    if (frame == NULL)
        return false;
    
    /* set zero */
    memset (frame, 0, PGSIZE);

    /* Load this page. */
    page_install = pagedir_get_page (thread_current()->pagedir, entry->upage) == NULL
          && pagedir_set_page (thread_current()->pagedir, entry->upage, frame, entry->writable);
    if (!page_install)
    {
        frame_deallocate (frame, true);
        return false;
    }

    return page_install;
}

struct s_pte *
grow_stack(void* page)
{
    struct thread *t;
    struct s_pte* pte;

    t = thread_current ();

    pte = (struct s_pte *) malloc (sizeof(struct s_pte));
    if (pte == NULL)
        return NULL;

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
    pte->mmap_id;
    pte->swap_slot;

    hash_insert(t->s_page_table, &(pte->elem));

    return pte;     
}
