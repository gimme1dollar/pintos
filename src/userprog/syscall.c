#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "vm/page.h"

bool
check_mem (void *addr)
{
  //return addr != NULL && is_user_vaddr(addr);
  return addr != NULL && is_vm_user_vaddr(addr);
}

bool
check_vm_mem (void *addr)
{
  return check_mem(addr) && (s_page_lookup (pg_round_down(addr)) != NULL);
}

bool
check_buffer (void *buffer, unsigned size)
{
  int s;
  //printf("%#08X\n", buffer);
  //printf("%#08X\n", pg_round_down(buffer));
  for (s = 0; s < size; s++) 
  {
    if (! check_vm_mem (buffer+s)) {
      //printf("### s is %d\n", s);
      return false;
    }
  }

  return true;
}

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault occurred.
   (referenced from official document 3.1.5)
   */
int
load_mem (const uint8_t *addr)
{
  if (!check_mem (addr)) return -1;

  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:": "=&a" (result) : "m" (*addr));
  return result;
}

void
read_mem (void *dst, void *src, size_t size)
{
  int value;
  size_t i;

  for (i = 0; i < size; i++)
  {
    value = load_mem (src + i);
    if(value == -1)
    {
      sys_exit(-1, NULL);
    }

    *(char*)(dst + i) = value & 0xff;
  }

  return;
}


void
syscall_init (void)
{
  lock_init (&syscall_handler_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  int syscall_number;
  void *esp = f->esp;

  // read memory
  read_mem (&syscall_number, esp, sizeof(int));
  thread_current ()->curr_esp = f->esp;

  //printf ("*** system call with syscall_number %d ***\n", syscall_number);

  // actions
  switch(syscall_number) {
    case SYS_HALT:
    {
      sys_halt (); 
      break;
    }
    case SYS_EXIT:
    {
      int exit_code;
      read_mem (&exit_code, esp+4, sizeof(int));

      sys_exit (exit_code, f);
      break;
    }
    case SYS_EXEC:
    {
      void *cmd;
      read_mem (&cmd, esp+4, sizeof(void *));

      sys_exec (cmd, f);
      break;
    }
    case SYS_WAIT:
    {
      int tid;
      read_mem (&tid, esp+4, sizeof(int));

      sys_wait (tid, f);
      break;
    }
    case SYS_CREATE:
    {
      char * file;
      size_t size;
      read_mem(&file, esp+4, sizeof(file));
      read_mem(&size, esp+8, sizeof(size));

      sys_create(file, size, f);
      break;
    }
    case SYS_REMOVE:
    {
      char *file;
      read_mem(&file, esp+4, sizeof(file));

      sys_remove(file, f);
      break;
    }
    case SYS_OPEN:
    {
      char *file;
      read_mem(&file, esp+4, sizeof(file));

      sys_open(file, f);
      break;
    }
    case SYS_FILESIZE:
    {
      int fd;
      read_mem(&fd, esp+4, sizeof(fd));

      sys_filesize(fd, f);
      break;
    }
    case SYS_READ:
    {
      int fd;
      void *buffer;
      unsigned size;
      read_mem(&fd, esp+4, sizeof(fd));
      read_mem(&buffer, esp+8, sizeof(buffer));
      read_mem(&size, esp+12, sizeof(size));
      //printf("size is %d\n", size);

      sys_read(fd, buffer, size, f);
      break;
    }
    case SYS_WRITE:
    {
      int fd;
      void *buffer;
      unsigned size;
      read_mem(&fd, esp+4, sizeof(fd));
      read_mem(&buffer, esp+8, sizeof(buffer));
      read_mem(&size, esp+12, sizeof(size));

      sys_write(fd, buffer, size, f);
      break;
    }
    case SYS_SEEK:
    {
      int fd;
      unsigned position;
      read_mem(&fd, esp+4, sizeof(fd));
      read_mem(&position, esp+8, sizeof(position));

      sys_seek(fd, position, f);
      break;
    }
    case SYS_TELL:
    {
      int fd;
      read_mem(&fd, esp+4, sizeof(fd));

      sys_tell(fd, f);
      break;
    }
    case SYS_CLOSE:
    {
      int fd;
      read_mem(&fd, esp+4, sizeof(fd));

      sys_close(fd, f);
      break;
    }
    case SYS_MMAP:
    {
      int fd;
      void *addr;
      read_mem(&fd, esp+4, sizeof(fd));
      read_mem(&addr, esp+8, sizeof(addr));

      sys_mmap(fd, addr, f);
      break;
    }
    case SYS_MUNMAP:
    {
      int map_id;
      read_mem(&map_id, esp+4, sizeof(map_id));

      sys_munmap(map_id, f, true);
      break;
    }
  }
}

void
sys_halt (void)
{
  shutdown_power_off ();

  return;
}

void
sys_exit (int exit_code, struct intr_frame *f UNUSED)
{
  //printf("sys_exit %d\n", exit_code);
  struct thread *cur;
  int i;

  if(lock_held_by_current_thread(&syscall_handler_lock)){
    lock_release (&syscall_handler_lock);
  }

  cur = thread_current();
  if (cur->child_elem != NULL)
    cur->child_elem->exit_code = exit_code;

  for (i = 3; i < 131; i++)
  {
    if(cur->file_des[i] != NULL)
      sys_close(i, f);
  }

  printf("%s: exit(%d)\n", cur->name, exit_code);
  thread_exit();

  return;
}

void
sys_exec (void *cmd, struct intr_frame *f)
{
  //printf("sys_exec\n");

  //lock_acquire(&syscall_handler_lock);
  f->eax = process_execute ((char*)cmd);
  //lock_release(&syscall_handler_lock);

  return;
}

void
sys_wait (int tid, struct intr_frame *f)
{
  //printf("sys_wait\n");
  int ret;

  ret = process_wait(tid);
  f->eax = ret;

  return;
}

void
sys_create (char *file, size_t size, struct intr_frame *f)
{
  //check_mem (name);
  if(file == NULL)
    sys_exit(-1, NULL);

  lock_acquire (&syscall_handler_lock);
  f->eax = filesys_create(file, size);
  lock_release(&syscall_handler_lock);

  return;
}

void
sys_remove (char *file, struct intr_frame *f)
{
  if(file == NULL)
    sys_exit(-1, NULL);

  lock_acquire (&syscall_handler_lock);
  f->eax = filesys_remove(file);
  lock_release(&syscall_handler_lock);

  return;
}

void
sys_open (char *file, struct intr_frame *f)
{
  //printf("sysopen\n");
  if(file == NULL)
    sys_exit(-1, NULL);

  int fd = thread_current()->next_fd;
  //printf("before lock acquire\n");
  lock_acquire (&syscall_handler_lock);
  //printf("after lock acquire\n");
  struct file* open_f = filesys_open(file);
  //printf("after file open\n");

  if(open_f == NULL)
  {
    //printf("file is null\n");
    f->eax = -1;
  }
  else
  {
    thread_current()->file_des[fd] = open_f;
    thread_current()->next_fd += 1;
    f->eax = fd;
  }
  lock_release(&syscall_handler_lock);

  return;
}

void
sys_filesize (int fd, struct intr_frame* f)
{
  struct thread *cur;

  cur = thread_current ();
  f->eax = file_length (cur->file_des[fd]);

  return;
}

void
sys_read (int fd, void *buffer, unsigned size, struct intr_frame *f)
{
  int i;
  
  if(!lock_held_by_current_thread(&syscall_handler_lock)){
    lock_acquire (&syscall_handler_lock);
  }
  if (fd == 0) // stdin
  {
    for(i = 0; i < size; i++)
    {
      ((char *)buffer)[i] = input_getc();
      if(((char *)buffer)[i] == '\0')
        break;
    }
    f->eax = i;
  }
  else if(fd > 2) // file
  {
    if(thread_current()->file_des[fd] == NULL || !check_buffer(buffer, size)){
      //printf("buffer is not ok %d\n", !check_buffer(buffer, size));
      if(lock_held_by_current_thread(&syscall_handler_lock)){
        lock_release (&syscall_handler_lock);
      }
      sys_exit(-1, NULL);
    }

    f->eax = file_read(thread_current()->file_des[fd], buffer, size);
  }
  else
    f->eax = -1;
  if(lock_held_by_current_thread(&syscall_handler_lock)){
    lock_release (&syscall_handler_lock);
  }

  return;
}

void
sys_write (int fd, void *buffer, unsigned size, struct intr_frame *f)
{
  if (!check_mem (buffer))
    sys_exit (-1, NULL);

  if(!lock_held_by_current_thread(&syscall_handler_lock)){
    lock_acquire (&syscall_handler_lock);
  }
  if (fd == 1) // stdout
  {
    putbuf(buffer, size);
    f->eax = size;
  }
  else if (fd > 2)
  {
    if(thread_current()->file_des[fd] == NULL || !check_buffer(buffer, size)) {
      if(lock_held_by_current_thread(&syscall_handler_lock)){
        lock_release (&syscall_handler_lock);
      }
      sys_exit(-1, NULL);
    }

    f->eax = file_write(thread_current()->file_des[fd], buffer, size);
  }
  else
    f->eax = -1;
  if(lock_held_by_current_thread(&syscall_handler_lock)){
    lock_release (&syscall_handler_lock);
  }

  return;
}

void
sys_seek (int fd, unsigned position, struct intr_frame *f UNUSED)
{
  struct thread *cur;

  cur = thread_current ();
  file_seek(cur->file_des[fd], position);
}

void
sys_tell (int fd, struct intr_frame *f)
{
  struct thread *cur;

  cur = thread_current ();
  f->eax = file_tell(cur->file_des[fd]);
}

void
sys_close (int fd, struct intr_frame *f UNUSED)
{
  struct thread *cur;

  cur = thread_current ();
  file_close (cur->file_des[fd]);
  cur->file_des[fd] = NULL;
}

void
sys_mmap(int fd, void* addr, struct intr_frame *f)
{
  struct thread *t;
  struct s_pte* pte;
  struct mmap_elem *me;
  struct file *file;
  uint32_t read_bytes;
  uint32_t zero_bytes;
  size_t page_read_bytes;
  size_t page_zero_bytes;
  off_t ofs;
  void* upage;
  int i_iter, i_end;

  lock_acquire (&syscall_handler_lock);

  /* file descriptor below is stdio */
  if(fd < 2) {
    f->eax = -1;
    lock_release(&syscall_handler_lock);
    return;
  }

  /* addr should not be zero & be algiend */
  if(addr != pg_round_down(addr) || addr == 0) {
    f->eax = -1;
    lock_release(&syscall_handler_lock);
    return;
  }

  t = thread_current ();

  if (t->file_des[fd] == NULL) {
    f->eax = -1;
    lock_release(&syscall_handler_lock);
    return;
  }

  /* get info from the file */
  upage = pg_round_down(addr);
  read_bytes = file_length(t->file_des[fd]); // length of file
  zero_bytes = PGSIZE - (read_bytes % PGSIZE);
  ofs = 0;

  /* file is of length zero */
  if(read_bytes == 0) {
    f->eax = -1;
    lock_release(&syscall_handler_lock);
    return;
  }


  /* same address */
  i_end = read_bytes / PGSIZE;
  i_end += read_bytes % PGSIZE == 0 ? 0 : 1;
  for(i_iter = 0; i_iter < i_end; i_iter++) {
      if(s_page_lookup(upage + i_iter * PGSIZE) != NULL) {
        f->eax = -1;
        lock_release(&syscall_handler_lock);
        return;
      }
  } 

  /* create mmap_list_entry for thread */
  me = (struct mmap_elem *)malloc(sizeof(struct mmap_elem));
  if (me == NULL) {
    f->eax = -1;
    lock_release(&syscall_handler_lock);
    return;
  }
  list_init (&me->s_pte_list);
  me->mmap_id = t->next_mmap;

  file = file_reopen(t->file_des[fd]); // reopen file

  /* make s_pte and insert into mmap_list of thread */
  while (read_bytes > 0 || zero_bytes > 0)
  {
    page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    page_zero_bytes = PGSIZE - page_read_bytes;

    /* Make a page table entry */
    pte = (struct s_pte *) malloc (sizeof(struct s_pte));
    if (pte == NULL) {
      f->eax = -1;
      return;
    }

    pte->tid = t->tid;
    pte->type = s_pte_type_MMAP;
    pte->table_number = upage;
    pte->writable = true;

    pte->file = file;
    pte->upage = upage;
    pte->file_page;
    pte->page_offset = ofs;
    pte->read_bytes = page_read_bytes;
    pte->zero_bytes = page_zero_bytes;
    pte->mmap_id = me->mmap_id;
    pte->mmap_elem;
    pte->swap_slot;
    
    /* Add to thread->mmap_list */
    list_push_back (&(me->s_pte_list), &(pte->mmap_elem));

    /* Add the page table entry */
    //printf("*** put s_pte mmap_id %d with %#08X\n", pte->mmap_id, pte->upage);
    hash_insert (t->s_page_table, &(pte->elem));
    //printf("*** in hash_table s_pte mmap_id %d\n", s_page_lookup(pte->upage)->mmap_id);

    /* Advance. */
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    ofs += page_read_bytes;
    upage += PGSIZE;
  }

  list_push_back(&t->mmap_list, &(me->elem));
  f->eax = t->next_mmap;
  t->next_mmap++;

  lock_release(&syscall_handler_lock);
  return;
}

void
sys_munmap(int map_id, struct intr_frame *f, bool from_syscall)
{
  //printf("in sys_munmap \n");

  struct thread *t;
  struct list_elem *e;
  struct mmap_elem *me;
  struct s_pte *pte;
  bool me_found;
  
  lock_acquire (&syscall_handler_lock);
  t = thread_current ();

  /* find the target mmap_elem in the mmap_list to get list of s_pte's */
  
  //printf("iteration on mmap_list start \n");
  me_found = false;
  for (e = list_begin (&(t->mmap_list)); e != list_end (&(t->mmap_list));
       e = list_next (e))
    {
      me = list_entry (e, struct mmap_elem, elem);

      if (me->mmap_id == map_id) {
        // found mmap_elem
        me_found = true;
        break;
      }
    }
  //printf("iteration on mmap_list over \n");

  if (!me_found) 
  {
    lock_release(&syscall_handler_lock);
    return;
  }

  /* unmap all of the s_pte in the mmap_list of mmap_elem */
  //printf("removing s_pte in mmap_list start \n");
  for (e = list_begin (&me->s_pte_list); e != list_end (&me->s_pte_list); 
       e = e)
    {
      pte = list_entry (e, struct s_pte, mmap_elem);
      if (pagedir_is_dirty(t->pagedir, pte->upage))
        {
          //printf("dirty pagedir\n");
          file_write_at (pte->file, pte->upage, pte->read_bytes, pte->page_offset);    
        }

      /*  delete from page_table */
      //printf("removing e\n");
      e = list_remove (e); // remove s_pte from mmap_list in thread
      s_page_delete (t->s_page_table, &(pte->elem)); // remove s_pte from s_page_table in thread
      //printf("deletion complete\n");
    }
  //printf("removing s_pte in mmap_list over \n");

  /* finally remove mmap_list entry from mmap_list in thread */
  list_remove (&me->elem); 
  free(me);

  lock_release(&syscall_handler_lock);
  return;
}