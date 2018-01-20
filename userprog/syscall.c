#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#define SYSCALL_COUNT 13

typedef int pid_t;

int fid = 2;

static struct lock fid_lock;

static struct lock files_list_lock;

struct file_elem{
  struct list_elem elem;
  void* data;
  int fid;
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

static struct file* get_file (int fd);
static int allocate_fid (void);
static struct list_elem *get_list_elem (int fd);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  lock_init (&fid_lock);  /* Initialize fid lock. */
  lock_init (&files_list_lock);

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

  lock_acquire (&files_list_lock);
  struct list_elem *e;
  struct list* process_file_list = &(get_process (thread_tid ())->files);
  struct list_elem *next;

  for (e = list_begin (process_file_list); e != list_end (process_file_list);
       e = next)
    {
      next = list_next (e);
      struct file_elem *t = list_entry (e,
      struct file_elem, elem);

      filesys_acquire_external_lock();
      file_close ((struct file*) t->data);
      filesys_release_external_lock();

      free(e);
    }
  lock_release (&files_list_lock);

  exit (status);
}

static void
sys_exec_handle (struct intr_frame *f)
{
  char *cmd_line = (char *)get_user_four_byte (f->esp + 4);

  /* Check for pointer validity. */
  if (cmd_line >= PHYS_BASE || get_user (cmd_line) == -1)
     exit (-1);

  filesys_acquire_external_lock();
  f->eax = process_execute (cmd_line);
  filesys_release_external_lock();
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

  filesys_acquire_external_lock();
  f->eax = filesys_create (file, initial_size);
  filesys_release_external_lock();
}

static void
sys_remove_handle (struct intr_frame *f)
{
  char *file = (char *)get_user_four_byte (f->esp + sizeof(void*));
  if (file >= PHYS_BASE || get_user (file) == -1) exit (-1);

  filesys_acquire_external_lock();
  f->eax = filesys_remove (file);
  filesys_release_external_lock();
}

static int
allocate_fid ()
{
  lock_acquire (&fid_lock);
  fid++;
  int temp = fid;
  lock_release (&fid_lock);

  return temp;
}

static void
sys_open_handle (struct intr_frame *f)
{
  char *file = (char *)get_user_four_byte (f->esp + sizeof(void*));
  if (file >= PHYS_BASE || get_user (file) == -1) exit (-1);

  f->eax = -1;  /* error value, will be overwritten in case of succ */

  filesys_acquire_external_lock();
  void *file_ptr = filesys_open (file);
  filesys_release_external_lock();

  if(!file_ptr)
     return;

  struct file_elem *elem = (struct file_elem *)malloc(sizeof(struct file_elem));
  elem->data = file_ptr;
  elem->fid = allocate_fid();
  struct process *p = get_process (thread_tid ());

  lock_acquire (&files_list_lock);
  list_push_back (&p->files, &elem->elem);
  lock_release (&files_list_lock);

  f->eax = elem->fid;
}

static struct file*
get_file (int fd)
{
  lock_acquire (&files_list_lock);
  struct list_elem *e;
  struct list* process_file_list = &(get_process (thread_tid ())->files);

  for (e = list_begin (process_file_list); e != list_end (process_file_list);
       e = list_next (e))
    {
      struct file_elem *t = list_entry (e,
      struct file_elem, elem);
      if(fd == t->fid){
        lock_release (&files_list_lock);
        return (struct file*) t->data;
      }
    }
  lock_release (&files_list_lock);
  return NULL;
}

static void
sys_filesize_handle (struct intr_frame *f)
{
  int fd = (int)get_user_four_byte (f->esp + sizeof(void*));

  f->eax = 0xffffffff;  /* error value, will be overwritten in case of succ */

  struct file* file_object =  get_file(fd);
  if(file_object == NULL)   /* try to access to wrong file */
     return;

  filesys_acquire_external_lock();
  f->eax = file_length(file_object);  /* get the size */
  filesys_release_external_lock();
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
      for(unsigned i=0;i<size;i++)
        *(char*)(buffer+i) = input_getc();

      f->eax = size;
      return;
    }

  struct file* file_object =  get_file(fd);

  if(file_object == NULL)    /* try to access to wrong file */
    return;

  filesys_acquire_external_lock();
  f->eax = file_read (file_object, buffer, size); /* read */
  filesys_release_external_lock();
}

static void
sys_write_handle (struct intr_frame *f)
{
  int fd = (int) get_user_four_byte (f->esp + 4);
  void *buffer = (void *) get_user_four_byte (f->esp + 8);
  unsigned size =  (unsigned) get_user_four_byte(f->esp + 12);

  if (buffer >= PHYS_BASE || get_user (buffer) == -1) exit (-1);
  f->eax = -1;  /* error value, will be overwritten in case of succ */


  /* Check for pointer validity. */
  if (buffer + size - 1 >= PHYS_BASE || get_user (buffer + size - 1) == -1)
     exit (-1);

  if (fd == 1)
    {
      filesys_acquire_external_lock();
      putbuf (buffer,size);
      filesys_release_external_lock();

      f->eax = size;
      return;
    }

  struct file* file_object =  get_file(fd);
  if(file_object == NULL)  return;  /* try to access to wrong file */

  filesys_acquire_external_lock();
  f->eax = file_write (file_object, buffer, size); /* write */
  filesys_release_external_lock();
}

static void
sys_seek_handle (struct intr_frame *f)
{
  int fd = get_user_four_byte (f->esp + 4);
  unsigned position = (unsigned) get_user_four_byte (f->esp + 8);

  struct file* file_object =  get_file(fd);
  if(file_object == NULL)  return;  /* try to access to wrong file */

  filesys_acquire_external_lock();
  file_seek(file_object,position);
  filesys_release_external_lock();
}

static void
sys_tell_handle (struct intr_frame *f)
{
  int fd = (int) get_user_four_byte (f->esp + 4);

  struct file* file_object =  get_file(fd);
  if(file_object == NULL)  return;  /* try to access to wrong file */

  filesys_acquire_external_lock();
  f->eax = file_tell(file_object);
  filesys_release_external_lock();
}

static struct list_elem*
get_list_elem (int fd)
{
  lock_acquire (&files_list_lock);
  struct list_elem *e;
  struct list* process_file_list = &(get_process (thread_tid ())->files);

  for (e = list_begin (process_file_list); e != list_end (process_file_list);
       e = list_next (e))
    {
      struct file_elem *t = list_entry (e,
      struct file_elem, elem);
      if(fd == t->fid){
        lock_release (&files_list_lock);
        return e;
      }
    }

  lock_release (&files_list_lock);
  return NULL;
}

static void
sys_close_handle (struct intr_frame *f)
{
  int fd = get_user_four_byte (f->esp + 4);

  filesys_acquire_external_lock();
  file_close (get_file (fd));
  filesys_release_external_lock();

  if(get_list_elem(fd) != NULL){
    struct list_elem *to_be_removed = get_list_elem(fd);
    lock_acquire (&files_list_lock);
    list_remove (to_be_removed);
    lock_release (&files_list_lock);
  }
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
