#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "userprog/syscall.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
static void argument_passing (char **args, int count, void **esp);
struct thread_arg
{
  char *file_name;
  struct thread *parent;
};


/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name)
{
  //printf("in process_exectue\n");
  struct thread *cur;
  char *fn_copy;
  tid_t tid;
  char *thread_name, *save_ptr;
  struct thread_arg *arg;
  int i;

  /* semaphore for loading child process */
  cur = thread_current ();
  cur->load_sema = (struct semaphore *) malloc (sizeof (struct semaphore));
  sema_init (cur->load_sema, 0);
  cur->is_loaded = 0;
  
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
  {
    free (cur->load_sema);
    return TID_ERROR;
  }
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Create a new thread to execute FILE_NAME. */
  thread_name = strtok_r (fn_copy, " ", &save_ptr);

  /* build argument for start_process */
  arg = (struct thread_arg *) malloc (sizeof(struct thread_arg));
  arg->file_name = palloc_get_page (0);
  strlcpy(arg->file_name, file_name, PGSIZE);
  //arg->file_name = fn_copy;
  arg->parent = cur;

  tid = thread_create (thread_name, PRI_DEFAULT, start_process, arg);
  //printf("thread created!\n");

  if (tid != TID_ERROR)
  {
    sema_down(cur->load_sema);
  }

  if(cur->is_loaded != 1) {
    palloc_free_page (fn_copy);
    palloc_free_page (arg->file_name);
    free (arg);
    free(cur->load_sema);
    return -1;
  }
  palloc_free_page (fn_copy);
  palloc_free_page (arg->file_name);
  free (arg);
  free(cur->load_sema);
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */

static void
start_process (void *arg)
{
  //printf("in start process\n");
  struct thread *parent, *child;
  char *file_name;

  char **args;
  struct intr_frame if_;
  bool success;
  char *token, *save_ptr;
  int count = 0;

  /* parse thread_arg */
  file_name = ((struct thread_arg *)arg)->file_name;
  parent = ((struct thread_arg *)arg)->parent;
  child = thread_current ();

  /* parse arguments list of the userprogram */
  args = palloc_get_page (0);
  if (args == NULL)
    return TID_ERROR;

  for(token = strtok_r (file_name, " ", &save_ptr); token != NULL; token = strtok_r (NULL, " ", &save_ptr))
  {
    args[count] = token;
    count++;
  }

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  /* init page_table */
  child->s_page_table = malloc(sizeof(struct hash));
  s_page_init(child->s_page_table);

  frame_init ();
  swap_init ();

  /* load */
  //printf("before load\n");
  success = load (args[0], &if_.eip, &if_.esp);
  //printf("after load\n");
  parent->is_loaded = success;

  if(success)
  {
    argument_passing (args, count, &if_.esp);
    //hex_dump(if_.esp, if_.esp, PHYS_BASE - if_.esp, true);

    struct child_elem *ce = malloc (sizeof(struct child_elem));
    ce->exit_code = -1;
    sema_init (&ce->wait_sema, 0);
    ce->tid = child->tid;
    child->child_elem = ce;
    list_push_back (&parent->child_list, &ce->elem);

    sema_up (parent->load_sema);
  }
  else /* If load failed, quit. */
  {
    sema_up (parent->load_sema);
    sys_exit (-1, NULL);
  }

  palloc_free_page (args);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Stack arguemtns
   */
static void
argument_passing (char **args, int count, void **esp)
{
  char* address[100];
  int i, j, total = 0, word_align;

  /* argv */
  for(i = count - 1; i >= 0; i--)
  {
    for(j = strlen(args[i]); j >= 0; j--)
    {
      *esp = *esp - 1;
      **(char **)esp = args[i][j];
      total++;
    }
    address[i] = *esp;
  }

  /* word align */
  word_align = 4 - total % 4;
  word_align = word_align == 4 ? 0 : word_align;
  *esp = *esp - word_align;

  *esp = *esp - sizeof(char*);
  memset(*esp, 0, sizeof(char*));

  /* argv lists */
  for(i = count - 1; i >= 0; i--)
  {
    *esp = *esp - sizeof(char*);
    memcpy(*esp, &address[i], sizeof(char*));
  }

  /* argv */
  *esp = *esp - sizeof(char**);
  **(int **)esp = (int)(*esp + sizeof(char**));

  /* argc */ 
  *esp = *esp - sizeof(int);
  **(int **)esp = count;

  /* return address */
  *esp = *esp - sizeof(void*);
  **(int **)esp = 0;
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid)
{
  //printf("in process_wait of tid %d\n", thread_current () -> tid);

  int exit_code;
  struct list *child_list;
  struct child_elem *ce;
  struct list_elem *le;

  /* find child with same child_tid */
  child_list = &(thread_current ()->child_list);

  //printf("child_list length %d of tid %d\n", list_size (child_list), thread_current() -> tid);
  if (!list_empty (child_list)) {
    for (le = list_front(child_list); le != list_end(child_list); le = list_next(le)) {
      ce = list_entry (le, struct child_elem, elem);
      if(ce->tid == child_tid) {
        // wait for child to be exited (sema_up will be executed in thread_exit)
        sema_down (&ce->wait_sema);

        // get exit_code
        exit_code = ce->exit_code;

        // remove child from child_list
        list_remove (&(ce->elem));
        free (ce);

        return exit_code;
      }
    }
  }

  return -1;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
  int iterator;

  if(cur->run_file != NULL)
  {
    file_allow_write (cur->run_file);
    file_close (cur->run_file);
  }
  
  //printf("unmap on process exit\n");
  for (iterator = 0; iterator < cur->next_mmap; iterator++) 
  {
    sys_munmap(iterator, NULL, false);
  }
  s_page_free(cur->s_page_table);

  if(cur->s_page_table != NULL)
  {
    free(cur->s_page_table);
  }
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL)
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);

static bool lazy_setup_stack (void **esp);
static bool lazy_load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp)
{
  //printf("in load \n");
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  lock_acquire(&syscall_handler_lock);
  //printf("before file open\n");
  file = filesys_open (file_name);
  //printf("after file open\n");
  if (file == NULL)
    {
      lock_release(&syscall_handler_lock);
      printf ("load: %s: open failed\n", file_name);
      goto done;
    }
  file_deny_write(file);
  lock_release(&syscall_handler_lock);
  t->run_file = file;

  /* Read and verify executable header. */
  //printf("first file read\n");
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done;
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file))
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              //printf("lazy loading \n");
              if (!lazy_load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!lazy_setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  //file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0)
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false;
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable))
        {
          palloc_free_page (kpage);
          return false;
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

static bool
lazy_load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  //printf("in lazy loading\n");
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  struct thread *t;
  struct s_pte *pte;

  //lock_acquire (&load_lock);
  t = thread_current ();
  while (read_bytes > 0 || zero_bytes > 0)
    {
     // printf("in while of lazy_loading\n");
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Make a page table entry */
      pte = (struct s_pte *) malloc (sizeof(struct s_pte));
      if (pte == NULL)
      {
        //lock_release (&load_lock);
        return false;
      }

      pte->tid = t->tid;
      pte->type = s_pte_type_FILE;
      pte->table_number = upage;
      
      pte->writable = writable;
      pte->file = file;
      pte->upage = upage;
      pte->file_page;
      pte->page_offset = ofs;
      pte->read_bytes = page_read_bytes;
      pte->zero_bytes = page_zero_bytes;
      pte->mmap_id;
      pte->swap_slot;
      
      //printf("table number: %d\n", pte->table_number);
      /* Add the page table entry */
      hash_insert (t->s_page_table, &(pte->elem));

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      ofs += page_read_bytes;
      upage += PGSIZE;
    }


  //printf("end of lazy laoding!\n");
  //lock_release (&load_lock);
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp)
{
  //printf("in setup_stack\n");
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL)
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
  

  //printf("out setup_stack\n");
  return success;
}

static bool
lazy_setup_stack (void **esp)
{
  //printf("in lazy_setup_stack\n");
  struct thread *t;
  struct s_pte *pte;
  bool success = false;
  uint8_t *upage;

  //lock_acquire (&load_lock);
  /* Make a s_pte */
  upage = pg_round_down(((uint8_t *) PHYS_BASE) - PGSIZE);
  //printf("######### lazy stack upage %#08X\n", upage);
  pte = grow_stack(upage);
  if(pte == NULL) 
    return false;
  else {
    success = true;
  }

  // /* insert the entry into table */
  // t = thread_current ();
  // hash_insert(t->s_page_table, &(pte->elem));
  
  /* set esp */
  if (success)
    *esp = PHYS_BASE;
  
  //lock_release (&load_lock);
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
