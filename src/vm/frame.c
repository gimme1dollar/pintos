#include "vm/frame.h"
#include "threads/palloc.h"

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
frame_init()
{
    if(frame_table == NULL){
        return;
    }

    frame_table = (struct hash *) malloc(sizeof(struct hash));
    hash_init(frame_table, frame_hash, frame_less, NULL);
    list_init(frame_list);

    return;
}

void
frame_destroy (struct hash_elem *e, void *aux)
{
    struct fte *entry;
    
    entry = hash_entry(e, struct fte, helem);  
    free(entry);

    return;
}

void 
frame_free(struct thread *t)
{
    struct fte *entry;
    /* free entry with having entry->tid == t->tid */


    /* free frame table if every entry is deleted */ 
    if (hash_empty (frame_table))
    {
        //list_remove(frame_list);
        hash_destroy(frame_table, frame_destroy);
        
        free(frame_table);
        free(frame_list);
    }

    return;
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
    frame_evict();

    kpage = palloc_get_page (flag); // get allocated again
  }

  /* make frame table entry */
  entry = (struct fte *)malloc(sizeof(struct fte));
  if(entry == NULL) 
  {
      return NULL;
  }

  entry->t = thread_current ();
  entry->frame_number = kpage;
  entry->kpage = kpage;
  entry->upage = upage;
  entry->s_pte = s_page_lookup(upage);
  entry->bit_not_evict = true; //don't ban with this flag on
  entry->bit_reference = 0;
  
  /* insert into hash table */
  hash_insert (frame_table, &(entry->helem));
  list_push_back (frame_list, &(entry->lelem));
  return kpage;
}

void
frame_deallocate()
{

}

void
frame_evict()
{
  struct fte *target;
  bool dirty;

  /* get target via clock algorithm */

  
  /* clear page mapping */
  pagedir_clear_page (target->t->pagedir, target->upage);

  /* swap out or free */ 
  switch (target->s_pte->type)
  {
        case s_pte_type_STACK:
            target->s_pte->type = s_pte_type_SWAP;
            break;
        case s_pte_type_FILE:
            target->s_pte->type = s_pte_type_SWAP;
            break;
        case s_pte_type_MMAP:
            free(target->s_pte);
            break;
        case s_pte_type_SWAP:
            break;
        default:
            break;
  }
  
  return;
}