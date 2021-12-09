#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

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

struct lock syscall_handler_lock;

void syscall_init (void);

bool check_mem (void *addr);
bool check_vm_mem (void *addr);
bool check_buffer (void *buffer, unsigned size);
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
void sys_tell(int , struct intr_frame *);
void sys_close(int , struct intr_frame * UNUSED);
void sys_mmap(int, void *, struct intr_frame *);
void sys_munmap(int, struct intr_frame *, bool);

#endif /* userprog/syscall.h */
