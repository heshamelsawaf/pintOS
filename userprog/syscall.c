#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#define SYSCALL_COUNT 13

static void syscall_handler (struct intr_frame *);

static int (*syscall_handlers[SYSCALL_COUNT]) (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  /* Initialize system calls function pointers. */
  syscall_handlers[SYS_HALT]     = &sys_halt_handle;
  syscall_handlers[SYS_EXIT]     = &sys_exit_handle;
  syscall_handlers[SYS_EXEC]     = &sys_exec_handle;
  syscall_handlers[SYS_WAIT]     = &sys_wait_handle;
  syscall_handlers[SYS_CREATE]   = &sys_create_handle;
  syscall_handlers[SYS_REMOVE]   = &sys_remove_handle;
  syscall_handlers[SYS_OPEN]     = &sys_open_handle;
  syscall_handlers[SYS_FILESIZE] = &sys_filesize_handle;
  syscall_handlers[SYS_READ]     = &sys_read_handle;
  syscall_handlers[SYS_WRITE]    = &sys_write_handle;
  syscall_handlers[SYS_SEEK]     = &sys_seek_handle;
  syscall_handlers[SYS_CLOSE]    = &sys_tell_handle;
  syscall_handlers[SYS_CLOSE]    = &sys_close_handle;
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
  printf ("system call!\n");
  thread_exit ();
}
