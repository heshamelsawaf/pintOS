#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#define SYSCALL_COUNT 13

typedef int pid_t;

static void syscall_handler (struct intr_frame *);

static void (*syscall_handlers[SYSCALL_COUNT]) (struct intr_frame *);

static void sys_halt_handle (struct intr_frame *);
static void sys_exit_handle (struct intr_frame *);
static void sys_exec_handle (struct intr_frame *);
static void sys_wait_handle (struct intr_frame *);

static void sys_create_handle (struct intr_frame *);
static void sys_remove_handle (struct intr_frame *);
static void sys_open_handle (struct intr_frame *);
static void sys_filesize_handle (struct intr_frame *);
static void sys_read_handle (struct intr_frame *);
static void sys_write_handle (struct intr_frame *);
static void sys_seek_handle (struct intr_frame *);
static void sys_tell_handle (struct intr_frame *);
static void sys_close_handle (struct intr_frame *);

static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static int get_user_four_byte (const uint8_t *uaddr);

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
  syscall_handlers[SYS_TELL]     = &sys_tell_handle;
  syscall_handlers[SYS_CLOSE]    = &sys_close_handle;
}

static void
sys_halt_handle (struct intr_frame *f UNUSED)
{
  shutdown_power_off ();
}

static void
sys_exit_handle (struct intr_frame *f)
{
   int status = get_user_four_byte (f->esp + 4);
   process_exit ();
   thread_exit ();
}


static void
sys_exec_handle (struct intr_frame *f)
{
  char *cmd_line = * (char **) (f->esp + 4);
  f->eax = process_execute (cmd_line);
}

static void
sys_wait_handle (struct intr_frame *f)
{
  pid_t pid =  (pid_t) get_user_four_byte (f->esp + 4);
  f->eax = process_wait (pid);
}


static void
sys_create_handle (struct intr_frame *f)
{
  char *file = * (char **) (f->esp + 4);
  unsigned initial_size = * (unsigned *) (f->esp + 8);
  f->eax = filesys_create (file, initial_size);
}

static void
sys_remove_handle (struct intr_frame *f)
{
  char *file = * (char **) (f->esp + 4);
  f->eax = filesys_remove (file);
}

static void
sys_open_handle (struct intr_frame *f)
{
  char *file = * (char **) (f->esp + 4);

}

static void
sys_filesize_handle (struct intr_frame *f)
{
  int fd = * (int *) (f->esp + 4);

}

static void
sys_read_handle (struct intr_frame *f)
{
  int fd = * (int *) (f->esp + 4);
}

static void
sys_write_handle (struct intr_frame *f)
{
  int fd = get_user_four_byte (f->esp + 4);
  void *buffer = (void *)get_user_four_byte (f->esp + 8);
  unsigned size =  (unsigned)get_user_four_byte(f->esp + 12);

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
sys_seek_handle (struct intr_frame *f)
{
  int fd = get_user_four_byte (f->esp + 4);
  unsigned position = (unsigned) get_user_four_byte (f->esp + 8);


}

static void
sys_tell_handle (struct intr_frame *f)
{
  int fd = get_user_four_byte (f->esp + 4);

}

static void
sys_close_handle (struct intr_frame *f)
{
  int fd = get_user_four_byte (f->esp + 4);

}

static void
syscall_handler (struct intr_frame *f)
{
  int syscall_key = get_user_four_byte (f->esp);
  syscall_handlers[syscall_key] (f);

  //TODO: Kill thread by thread_exit () if an error occured during system call
  // ex: page fault (user space validity), writing failed, ..etc.
}

/* Reads 4-bytes at user virtual address UADDR.
 UADDR must be below PHYS_BASE.
 Returns the 4-bytes value if successful, terminate the
 process o.w. */
static int
get_user_four_byte (const uint8_t *uaddr)
{
  int result = 0;
  for (int i = 0; i < 4; i++)
    {
      if ((void *) (uaddr + i) >= PHYS_BASE);
        //TODO: exit (-1);
      int ret_val = get_user (uaddr + i);
      if (ret_val == -1);
        //TODO: exit (-1);
      result |= (ret_val << (8*i));
    }
  return result;
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
