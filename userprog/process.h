#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

typedef int pid_t;

void process_init ();
tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (int);
void process_activate (void);

struct process {
  pid_t pid;
  struct list children_processes;
  struct list_elem elem;
  struct list_elem allelem;

  struct list files;
};

#endif /* userprog/process.h */
