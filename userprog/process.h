#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

typedef int pid_t;

void process_init (void);
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (int);
void process_activate (void);
/* Get process with given tid from list of all processes currently resident
in the system. */
struct process *get_process (tid_t tid);

struct process {
  pid_t pid;
  struct list files;

  struct list children_processes;
  struct list_elem elem;
  struct list_elem allelem;
  struct file *executable;
};

#endif /* userprog/process.h */
