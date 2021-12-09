#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"

/* return key */
unsigned 
frame_hash (const struct hash_elem *h, void *aux)
{
    struct fte *ft;
    
    ft = hash_entry(h, struct fte, helem);   

    return ft->frame_number;
}

bool 
frame_less (const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
    struct fte *fta, *ftb;

    fta = hash_entry(a, struct fte, helem);   
    ftb = hash_entry(b, struct fte, helem);  

    return fta->frame_number < ftb->frame_number;
}

void 
frame_init ()
{
    if(frame_table != NULL){
        return;
    }
    frame_table = (struct hash *) malloc(sizeof(struct hash));
    hash_init(frame_table, frame_hash, frame_less, NULL);
    lock_init (&frame_lock);

    frame_list = (struct list *)malloc(sizeof(struct list));
    list_init(frame_list);
    celem = NULL;

    return;
}

void
frame_destroy (struct hash_elem *e, void *aux)
{
    struct fte *entry;
    
    entry = hash_entry(e, struct fte, helem);  
    lock_acquire(&frame_lock);
    list_remove (&(entry->lelem));
    hash_delete(frame_table, &(entry->helem));
    lock_release(&frame_lock);
    free(entry);

    return;
}

void 
frame_free (struct s_pte *pte, bool flag)
{
    struct list_elem *elem;
    struct fte *target;

    for(elem = list_begin(frame_list); elem != list_end(frame_list);
        elem = list_next (frame_list)) {
            target = list_entry(elem, struct fte, lelem);
            if(target->s_pte == pte) {
                break;
            }
    }
    if(flag) palloc_free_page (target->frame_number);
    
    frame_destroy(&target->helem, NULL);

}


struct fte*
frame_lookup(uint8_t *frame_number)
{
    //printf("in s_page_lookup\n");
    struct thread *t;
    struct fte finder, *entry;
    struct hash_elem *target;

    /* find hash table with finder */
    finder.frame_number = frame_number;
    target = hash_find(frame_table, &(finder.helem));
    if(target == NULL)
    {
        return NULL;
    }

    /* find the entry from table */
    entry = hash_entry (target, struct fte, helem);
    if(entry == NULL)
    {
        return NULL;
    }
    return entry;
}

/* return kpage */
void *
frame_allocate(void *upage, enum palloc_flags flag)
{
  void *kpage;
  struct fte *entry;
  bool evicted;

  /* get allocation of kpage */
  kpage = palloc_get_page (flag);
  if (kpage == NULL) // palloc fail -> eviction
  {
    kpage = frame_evict(flag); // get allocated again
    //printf("allocate kage %#08X\n", kpage);
  }

  /* make frame table entry */
  entry = (struct fte *)malloc(sizeof(struct fte));
  if(entry == NULL) 
  {
      printf("entry is NULL\n");
      return NULL;
  }

  entry->frame_number = kpage;
  entry->t = thread_current ();
  entry->kpage = kpage;
  entry->upage = upage;
  entry->s_pte = s_page_lookup(upage);
  
  lock_acquire(&frame_lock);
  /* insert into hash table */
  hash_insert (frame_table, &(entry->helem));
  list_push_back (frame_list, &(entry->lelem));

  lock_release(&frame_lock);
  return kpage;
}

void
frame_deallocate(void *kpage, bool flag)
{
    struct fte *entry;

    entry = frame_lookup (pg_round_down (kpage));

    /* clear physical frame */
    if(flag) palloc_free_page (entry->frame_number);

    /* destrocy the entry */
    frame_destroy(&(entry->helem), NULL);

    return;
}

void *
frame_evict(enum palloc_flags flag)
{
  //printf("in frame_evict\n");
  void *kpage;
  struct fte *target, *candidate;
  struct s_pte *pte;
  bool dirty, found;
  int iterator, access_count;
  uint32_t swap_id;

  /* get target via clock algorithm */
  found = false;
  while(!found)
  {
    candidate = next_fte ();
    access_count = pagedir_is_accessed(candidate->t->pagedir, candidate->upage);
    if (access_count != 0)
    {
        pagedir_set_accessed (candidate->t->pagedir, candidate->upage, access_count-1);
        continue;
    }
    else
    {
        found = true;
        target = candidate;
    }
  }

  if (target == NULL)
  {
      printf("clock algorithm had wrong target\n");
      sys_exit(-1, NULL);
  }
  
  pte = target->s_pte;
  pagedir_clear_page (target->t->pagedir, target->upage);

  if(pte->file != NULL) {
    if (pagedir_is_dirty(target->t->pagedir, pte->upage))
    {
        //printf("dirty pagedir\n");
        if(!lock_held_by_current_thread(&syscall_handler_lock))
            lock_acquire (&syscall_handler_lock);
        file_write_at (pte->file, pte->upage, pte->read_bytes, pte->page_offset);
        if(lock_held_by_current_thread(&syscall_handler_lock))
            lock_release (&syscall_handler_lock);
    }
  }

  /* swap out */
  pte->swap_slot = swap_out(target->kpage);
  pte->prev_type = pte->type;
  pte->type = s_pte_type_SWAP;
  
  /* remove from frame table */
  frame_deallocate(target->kpage, true);
  
  /* reallocate new page */
  kpage = palloc_get_page (flag);
  return kpage;
}

struct fte *
next_fte (void)
{
    struct fte *entry;
    lock_acquire(&frame_lock);
    if (list_empty (frame_list))
    {
        lock_release(&frame_lock);
        printf("next_clock with empty frame_list\n");
        sys_exit(-1, NULL);
    }

    if (celem == NULL || celem == list_back (frame_list) ) 
    {
        celem = list_front (frame_list);
    }
    else 
    {
        celem = list_next (celem);
    }

    entry = list_entry (celem, struct fte, lelem);
    lock_release(&frame_lock);

    return entry;
}
