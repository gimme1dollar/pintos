#include "vm/frame.h"
#include "threads/palloc.h"

/* return key */
unsigned 
frame_hash (const struct hash_elem *h, void *aux)
{
    struct fte *ft;
    
    ft = hash_entry(h, struct fte, elem);   

    return ft->frame_number;
}

bool 
frame_less (const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
    struct fte *fta, *ftb;

    fta = hash_entry(a, struct fte, elem);   
    ftb = hash_entry(b, struct fte, elem);   

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

    return;
}

void
frame_destroy (struct hash_elem *e, void *aux)
{
    struct fte *ft;
    
    ft = hash_entry(e, struct fte, elem);  
    free(ft);

    return;
}

void 
frame_free()
{
    hash_destroy(frame_table, frame_destroy);
    free(frame_table);

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
    target = hash_find(frame_table, &(finder.elem));
    if(target == NULL)
    {
        return NULL;
    }

    /* find the entry from table */
    entry = hash_entry (target, struct fte, elem);
    if(entry == NULL)
    {
        return NULL;
    }

    return entry;
}

void
frame_allocate()
{

}

void
frame_deallocate()
{

}

void
frame_evict()
{
    
}