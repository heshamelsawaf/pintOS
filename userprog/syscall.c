#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#define SYSCALL_COUNT 13

typedef int pid_t;

int fid = 2;

static struct lock fid_lock;

struct file_elem{
  struct list_elem elem;
  void* data;
};

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

  lock_init (&fid_lock);  /* Initialize fid lock. */

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

void
exit (int status)
{
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit (status);
}

static void
sys_exit_handle (struct intr_frame *f)
{
   int status = get_user_four_byte (f->esp + 4);
   exit (status);
}

static void
sys_exec_handle (struct intr_frame *f)
{
  char *cmd_line = (char *)get_user_four_byte (f->esp + 4);

  /* Check for pointer validity. */
  if (cmd_line >= PHYS_BASE || get_user (cmd_line) == -1)
     exit (-1);
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
  char *file = (char *)get_user_four_byte (f->esp + sizeof(void*));
  if (file >= PHYS_BASE || get_user (file) == -1) exit (-1);
  unsigned initial_size = * (unsigned *) (f->esp + 2*sizeof(void*));
  f->eax = filesys_create (file, initial_size);
}

static void
sys_remove_handle (struct intr_frame *f)
{
  char *file = (char *)get_user_four_byte (f->esp + sizeof(void*));
  if (file >= PHYS_BASE || get_user (file) == -1) exit (-1);
  f->eax = filesys_remove (file);
}

static int
allocate_fid ()
{
  lock_acquire (&tid_lock);
  fid++;
  int temp = fid;
  lock_release (&tid_lock);

  return temp;
}

static void
sys_open_handle (struct intr_frame *f)
{
  char *file = (char *)get_user_four_byte (f->esp + sizeof(void*));
  if (file >= PHYS_BASE || get_user (file) == -1) exit (-1);

  f->eax = -1;  /* error value, will be overwritten in case of succ */

  void *file_ptr = filesys_open (file);
  if(!file_ptr) return;

  file_elem *elem = malloc(sizeof(file_elem));
  elem->data = file_ptr;
  elem->elem = allocate_fid();

  list_push_back (&(get_process (thread_tid ())->files), &(elem->elem));
  f->eax = elem->elem;
}

struct file*
get_file (int fd)
{
  struct list_elem *e;
  struct list* process_file_list = &(get_process (thread_tid ())->files);

  for (e = list_begin (&process_file_list); e != list_end (&process_file_list);
       e = list_next (e))
    {
      struct file_elem *t = list_entry (e,
      struct file_elem, allelem);
      if(fd == t->elem)
        return (struct file*) t->data;
    }

  return NULL;
}

static void
sys_filesize_handle (struct intr_frame *f)
{
  int fd = (int)get_user_four_byte (f->esp + sizeof(void*));

  f->eax = -1;  /* error value, will be overwritten in case of succ */

  struct file* file_object =  get_file(fd);
  if(file_object == NULL)  return;  /* try to access to wrong file */

  f->eax = file_length(file_object);  /* get the size */
}

static void
sys_read_handle (struct intr_frame *f)
{
  int fd = (int) get_user_four_byte (f->esp + sizeof(void*));
  void *buffer = (void *) get_user_four_byte (f->esp + 2*sizeof(void*));
  unsigned size = (unsigned) get_user_four_byte (f->esp + 3*sizeof(void*));

  if (buffer >= PHYS_BASE || get_user (buffer) == -1) exit (-1);
  f->eax = -1;  /* error value, will be overwritten in case of succ */

  /* Check for pointer validity. */
  if (buffer + size - 1 >= PHYS_BASE || get_user (buffer + size - 1) == -1)
     exit (-1);

  if(fd == 0)
    {
      for(int i=0;i<size;i++)
        *(char*)(buffer+i) = input_getc();

      f->eax = size;
      return;
    }

  struct file* file_object =  get_file(fd);
  if(file_object == NULL)  return;  /* try to access to wrong file */
  f->eax = file_read (file_object, buffer, size) /* read */
}

static void
sys_write_handle (struct intr_frame *f)
{
<<<<<<< HEAD
  int fd = get_user_four_byte (f->esp + 4);
  const void *buffer = (const void *)get_user_four_byte (f->esp + 8);
  unsigned size =  (unsigned)get_user_four_byte(f->esp + 12);
=======
  int fd = (int) get_user_four_byte (f->esp + 4);
  void *buffer = (void *) get_user_four_byte (f->esp + 8);
  unsigned size =  (unsigned) get_user_four_byte(f->esp + 12);

  if (buffer >= PHYS_BASE || get_user (buffer) == -1) exit (-1);
  f->eax = -1;  /* error value, will be overwritten in case of succ */
>>>>>>> 4e6eb1c462391003041b5b6e5d7bc82461ab6f0a


  /* Check for pointer validity. */
  if (buffer + size - 1 >= PHYS_BASE || get_user (buffer + size - 1) == -1)
     exit (-1);

<<<<<<< HEAD

  if (fd == STDOUT_FILENO)
=======
  if (fd == 1)
>>>>>>> 4e6eb1c462391003041b5b6e5d7bc82461ab6f0a
    {
      putbuf (buffer,size);
      f->eax = size;
      return;
    }

  struct file* file_object =  get_file(fd);
  if(file_object == NULL)  return;  /* try to access to wrong file */

  f->eax = file_write (file_object, buffer, size) /* read */
}

static void
sys_seek_handle (struct intr_frame *f)
{
  int fd = get_user_four_byte (f->esp + 4);
  unsigned position = (unsigned) get_user_four_byte (f->esp + 8);

  struct file* file_object =  get_file(fd);
  if(file_object == NULL)  return;  /* try to access to wrong file */

  file_seek(file_object,position);
}

static void
sys_tell_handle (struct intr_frame *f)
{
  int fd = (int) get_user_four_byte (f->esp + 4);

  struct file* file_object =  get_file(fd);
  if(file_object == NULL)  return;  /* try to access to wrong file */

  f->eax = file_tell(file_object);
}

static void
sys_close_handle (struct intr_frame *f)
{
  int fd = get_user_four_byte (f->esp + 4);
  file_close (get_file (fd));
  list_remove (&(get_file (fd)->elem));
}

static void
syscall_handler (struct intr_frame *f)
{
  int syscall_key = get_user_four_byte (f->esp);
  syscall_handlers[syscall_key] (f);
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
      if ((void *) (uaddr + i) >= PHYS_BASE)
         exit (-1);
      int ret_val = get_user (uaddr + i);
      if (ret_val == -1)
         exit (-1);
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
