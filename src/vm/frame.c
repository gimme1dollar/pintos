#include "vm/frame.h"

/* return key */
unsigned 
frame_hash (const struct hash_elem *h, void *aux)
{
    struct s_pte *s_pt;
    
    s_pt = hash_entry(h, struct s_pte, elem);   

    return s_pt->table_number;
}

bool 
frame_less (const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
    struct s_pte *s_pt_a, *s_pt_b;

    s_pt_a = hash_entry(a, struct s_pte, elem);   
    s_pt_b = hash_entry(b, struct s_pte, elem);   

    return s_pt_a->table_number < s_pt_b->table_number;
}

void 
frame_init(struct hash *target_table)
{
  hash_init(target_table, s_page_hash, s_page_less, NULL);
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
