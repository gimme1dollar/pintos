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
void sys_create(char *, size_t, struct intr_frame *);
void sys_remove(char *, struct intr_frame*);
void sys_open(char *, struct intr_frame *);
void sys_filesize(int , struct intr_frame*);
void sys_read(int , void *, unsigned, struct intr_frame *);
void sys_write(int , void *, unsigned, struct intr_frame *);
void sys_seek(int, unsigned, struct intr_frame * UNUSED);
void sys_tell(int , struct intr_frame *f);
void sys_close(int , struct intr_frame * UNUSED);

struct lock syscall_handler_lock;

bool
check_mem (void *addr)
{
  return addr != NULL && is_user_vaddr(addr);
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

  //printf ("system call with syscall_number %d\n", syscall_number);

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

  cur = thread_current();
  if (cur->child_elem != NULL)
    cur->child_elem->exit_code = exit_code;

  for (i = 0; i < 128; i++)
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

  lock_acquire(&syscall_handler_lock);
  f->eax = filesys_create(file, size);
  lock_release(&syscall_handler_lock);

  return;
}

void
sys_remove (char *file, struct intr_frame *f)
{
  if(file == NULL)
    sys_exit(-1, NULL);

  lock_acquire(&syscall_handler_lock);
  f->eax = filesys_remove(file);
  lock_release(&syscall_handler_lock);

  return;
}

void
sys_open (char *file, struct intr_frame *f)
{
  if(file == NULL)
    sys_exit(-1, NULL);

  int fd = thread_current()->next_fd;
  lock_acquire(&syscall_handler_lock);
  struct file* open_f = filesys_open(file);

  if(open_f == NULL)
    f->eax = -1;
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

  lock_acquire(&syscall_handler_lock);
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
  else if(fd >= 2)
  {
    if(thread_current()->file_des[fd] == NULL || !check_mem(buffer))
      sys_exit(-1, NULL);
    f->eax = file_read(thread_current()->file_des[fd], buffer, size);
  }
  else
    f->eax = -1;
  lock_release(&syscall_handler_lock);

  return;
}

void
sys_write (int fd, void *buffer, unsigned size, struct intr_frame *f)
{
  if (!check_mem (buffer))
    sys_exit (-1, NULL);

  lock_acquire(&syscall_handler_lock);
  if (fd == 1) // stdout
  {
    putbuf(buffer, size);
    f->eax = size;
  }
  else if (fd >= 2)
  {
    if(thread_current()->file_des[fd] == NULL)
      sys_exit(-1, NULL);
    f->eax = file_write(thread_current()->file_des[fd], buffer, size);
  }
  else
    f->eax = -1;
  lock_release(&syscall_handler_lock);

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
