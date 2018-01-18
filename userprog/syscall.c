#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#define SYSCALL_COUNT 13

typedef int pid_t;

static void syscall_handler (struct intr_frame *);

static void (*syscall_handlers[SYSCALL_COUNT]) (struct intr_frame *);

static void sys_write_handle (struct intr_frame *);
static void sys_exec_handle (struct intr_frame *);
static void sys_wait_handle (struct intr_frame *);

static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static int get_user_four_byte (const uint8_t *uaddr);

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
  pid_t pid =  (pid_t) get_user_four_byte (f->esp + 4);

  f->eax = process_wait (pid);
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
  for (int i=0; i<4; i++)
     {
       if ((void *)(uaddr+i) >= PHYS_BASE);
          //TODO: exit (-1);
       int retVal = get_user (uaddr + i);
       if (retVal == -1);
          //TODO: exit (-1);
       result |= (retVal << (8*i));
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
