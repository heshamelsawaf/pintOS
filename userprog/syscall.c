#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#define SYSCALL_COUNT 13

typedef int pid_t;

static void syscall_handler (struct intr_frame *);

static void (*syscall_handlers[SYSCALL_COUNT]) (struct intr_frame *);

static void sys_write_handle (struct intr_frame *);
static void sys_exec_handle (struct intr_frame *);
static void sys_wait_handle (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  /* Initialize system calls function pointers. */
  // syscall_handlers[SYS_HALT]     = &sys_halt_handle;
  // syscall_handlers[SYS_EXIT]     = &sys_exit_handle;
  syscall_handlers[SYS_EXEC]     = &sys_exec_handle;
  syscall_handlers[SYS_WAIT]     = &sys_wait_handle;
  // syscall_handlers[SYS_CREATE]   = &sys_create_handle;
  // syscall_handlers[SYS_REMOVE]   = &sys_remove_handle;
  // syscall_handlers[SYS_OPEN]     = &sys_open_handle;
  // syscall_handlers[SYS_FILESIZE] = &sys_filesize_handle;
  // syscall_handlers[SYS_READ]     = &sys_read_handle;
  syscall_handlers[SYS_WRITE]    = &sys_write_handle;
  // syscall_handlers[SYS_SEEK]     = &sys_seek_handle;
  // syscall_handlers[SYS_TELL]     = &sys_tell_handle;
  // syscall_handlers[SYS_CLOSE]    = &sys_close_handle;
}

static void
sys_exec_handle (struct intr_frame *f) {
  char *cmd_line = * (char **) (f->esp + 4);

  f->eax = process_execute (cmd_line);
}

static void
sys_wait_handle (struct intr_frame *f)
{
  pid_t pid = * (pid_t *) (f->esp + 4);
  
  // f->eax = process_wait (pid);
}

static void
sys_write_handle (struct intr_frame *f)
{
  int fd = * (int *) (f->esp + 4);
  void *buffer = * (char**) (f->esp + 8);
  unsigned size = * (unsigned *) (f->esp + 12);

  if (fd == STDOUT_FILENO)
    {
      putbuf (buffer,size);
      f->eax = size;
    }
  else
    {

    }
}

static void
syscall_handler (struct intr_frame *f)
{
  int syscall_key = * (int *) f->esp;
  syscall_handlers[syscall_key] (f);

  //TODO: Kill thread by thread_exit () if an error occured during system call
  // ex: page fault (user space validity), writing failed, ..etc.
}

/* Reads a byte at user virtual address UADDR.
 UADDR must be below PHYS_BASE.
 Returns the byte value if successful, -1 if a segfault
 occurred. */
static int
get_user (const uint8_t *uaddr)
{
int result;
asm ("movl $1f, %0; movzbl %1, %0; 1:"
     : "=&a" (result) : "m" (*uaddr));
return result;
}

/* Writes BYTE to user address UDST.
 UDST must be below PHYS_BASE.
 Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
int error_code;
asm ("movl $1f, %0; movb %b2, %1; 1:"
     : "=&a" (error_code), "=m" (*udst) : "q" (byte));
return error_code != -1;
}
