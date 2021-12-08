#include "vm/swap.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

void
swap_init (void)
{
    struct ste *te;
    size_t sectors_per_ste;
    uint32_t entry_id, entry_number;

    /* init swap_disk */
    swap_disk = block_get_role(BLOCK_SWAP);
    if(swap_disk == NULL) 
    {
        printf("swap_disk creation wrong\n");
        sys_exit(-1, NULL);
    } 

    /* init swap_list */
    sectors_per_ste = PGSIZE / BLOCK_SECTOR_SIZE;
    entry_number = block_size(swap_disk) / sectors_per_ste;

    list_init (swap_table);
    
    for(entry_id = 0; entry_id < entry_number; entry_id++)
    {
        te = (struct ste*)malloc(sizeof(struct ste));
        te->ste_id = entry_id;
        te->is_free = true;

        list_push_back (swap_table, &(te->elem));
    }

    return;
}

void 
swap_destroy (uint32_t index)
{
    struct list_elem *ste_elem;
    struct ste *entry;

    for(ste_elem = list_begin(swap_table); ste_elem != list_end(swap_table);
        ste_elem = list_next (ste_elem))
    {
        entry = list_entry (ste_elem, struct ste, elem);
        if(entry->ste_id == index)
        {
            entry->is_free = true;
        }
    }
}

void 
swap_free (void)
{
    struct list_elem *ste_elem;

    /* free swap_disk */
    free (swap_disk);

    /* free swap_table */
    for(ste_elem = list_begin(swap_table); ste_elem != list_end(swap_table);
        ste_elem = ste_elem)
    {
        ste_elem = list_remove(ste_elem);
    }
    free (swap_table);

    return;
}


void 
swap_in (uint32_t index, void *page)
{
  struct list_elem *ste_elem;
  struct ste *entry;
  uint32_t sector, count; 
  void *addr;

  if (!is_vm_user_vaddr (page))
  {
      printf("invalid page in swap_in \n");
      sys_exit(-1, NULL);
  }

  /* find the ste */
  for(ste_elem = list_begin(swap_table); ste_elem != list_end(swap_table);
      ste_elem = list_next (ste_elem))
  {
      entry = list_entry (ste_elem, struct ste, elem);
      if(entry->ste_id == index)
      {
        break;
      }
  }

  if(entry == NULL || entry->is_free == true)
  {
      printf("invalid index\n");
      sys_exit(-1, NULL);
  }

  /* read from swap_disk */
  for (count = 0; count < (PGSIZE / BLOCK_SECTOR_SIZE); count++) 
  {
      sector = index * (PGSIZE / BLOCK_SECTOR_SIZE) + count;
      addr = page + (BLOCK_SECTOR_SIZE * count);
      block_read (swap_disk, sector, addr);
  }

  entry->is_free = true;
  return;
}

uint32_t 
swap_out (void *page)
{
  struct list_elem *ste_elem;
  struct ste *entry;
  uint32_t sector, count; 
  void *addr;

  if (!is_vm_user_vaddr (page))
  {
      printf("invalid page in swap_in \n");
      sys_exit(-1, NULL);
  }

  /* find available sector */
  for(ste_elem = list_begin(swap_table); ste_elem != list_end(swap_table);
      ste_elem = list_next (ste_elem))
  {
      entry = list_entry (ste_elem, struct ste, elem);
      if(entry->is_free == true)
      {
        break;
      }
  }

  if(entry == NULL)
  {
      printf("can't find free swap table entry\n");
      return -1;
  }

  /* write to swap_disk */
  for (count = 0; count < (PGSIZE / BLOCK_SECTOR_SIZE); count++) 
  {
      sector = entry->ste_id * (PGSIZE / BLOCK_SECTOR_SIZE) + count;
      addr = page + (BLOCK_SECTOR_SIZE * count);
      block_write (swap_disk, sector, addr);
  }

  entry->is_free = false;
  return entry->ste_id;
}