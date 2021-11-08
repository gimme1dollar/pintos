#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"

bool check_mem (void *addr);
int  load_mem (const uint8_t *addr);
void read_mem (void *dst, void *src, size_t size);

static void syscall_handler (struct intr_frame *);
void sys_halt(void);
void sys_exit(int , struct intr_frame *);
void sys_exec(void *, struct intr_frame *);
void sys_wait(int , struct intr_frame *);
int sys_read(int fd, void *buffer, unsigned int size);
int sys_write(int fd, void *buffer, unsigned int size);


struct lock syscall_handler_lock;

bool
check_mem (void *addr)
{
  return addr != NULL || is_user_vaddr(addr);
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

  //printf ("system call!\n");

  // read memory
  read_mem (&syscall_number, esp, sizeof(int));

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

      sys_exec (cmd,f);
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
      break;
    case SYS_REMOVE:
      break;
    case SYS_OPEN:
      break;
    case SYS_FILESIZE:
      break;
    case SYS_READ:
    {
      int fd;
      void *buffer;
      unsigned int size;

      read_mem(&fd, esp+4, sizeof(int));
      read_mem(&buffer, esp+8, sizeof(void *));
      read_mem(&size, esp+12, sizeof(unsigned int));
      break;
    }
    case SYS_WRITE:
    {
      int fd;
      void *buffer;
      unsigned int size;

      read_mem(&fd, esp+4, sizeof(int));
      read_mem(&buffer, esp+8, sizeof(void *));
      read_mem(&size, esp+12, sizeof(unsigned int));
      sys_write(fd, buffer, size);
      break;
    }
    case SYS_SEEK:
      break;
    case SYS_TELL:
      break;
    case SYS_CLOSE:
      break;
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
  struct thread *cur;
  
  cur = thread_current();
  if (cur->child_elem != NULL)
    cur->child_elem->exit_code = exit_code;

  printf("%s: exit(%d)\n", thread_current()->name, exit_code);
  thread_exit();

  return;
}

void
sys_exec (void *cmd, struct intr_frame *f)
{
  int ret; 

  check_mem (cmd);

  lock_acquire(&syscall_handler_lock);
  ret = process_execute ((char*)cmd);
  f->eax = ret;
  lock_release(&syscall_handler_lock);

  return;
}

void 
sys_wait(int tid, struct intr_frame *f)
{
  int ret;
  
  ret = process_wait(tid);
  f->eax = ret;

  return;
}

int
sys_read(int fd, void *buffer, unsigned int size)
{
  return 0;
}

int
sys_write(int fd, void *buffer, unsigned int size)
{
  if (fd == 1)
  {
    putbuf(buffer, size);
    return size;
  }
  return -1;
}
