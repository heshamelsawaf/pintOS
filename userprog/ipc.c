#include <list.h>
#include <string.h>
#include "threads/synch.h"
#include "threads/malloc.h"
#include "userprog/ipc.h"


struct message {
  char *signature;            /* Signature that differentiates a message from other messages,
                                 (should always be unique). */
  int data;                   /* Data to be sent with the previously mentioned signature. */
  struct list_elem elem;
};

struct waiting_process {
  char *signature;            /* Signature to receive a message with. */
  struct semaphore sema;      /* Semaphore used to make processes wait on whenever they try to
                                 receive a message that is not yet put in list. */
  struct list_elem elem;
};

struct list sent_mail;
struct list waiting_list;


void ipc_init (void)
{
  list_init (&sent_mail);
  list_init (&waiting_list);
}

/* Get list_elem of message with given signature from sent mail, return NULL if
no such message is found in sent mail. */
struct list_elem *get_msg (char *signature)
{
  struct list_elem *e;

  for (e = list_begin (&sent_mail); e != list_end (&sent_mail); e = list_next (e))
    {
      struct message *msg = list_entry (e, struct message, elem);
      if (strcmp (msg->signature, signature) == 0)
        return e;
    }
    return NULL;
}

void ipc_send (char *signature, int data)
{
  struct list_elem *e;

  /* Construct a new message. */
  struct message *msg = malloc (sizeof (struct message));
  msg->signature = malloc (sizeof (char *));
  strlcpy (msg->signature, signature, strlen (signature) + 1);
  msg->data = data;
  /* Insert the newly constructed messsage to sent mail. */
  list_push_back (&sent_mail, &msg->elem);

  /* Iterate over waiting processes to find a process that waits for a message
  with the same signature. */
  for (e = list_begin (&waiting_list); e != list_end (&waiting_list); e = list_next (e))
    {
      struct waiting_process *proc = list_entry (e, struct waiting_process, elem);

      if (strcmp (signature, proc->signature) == 0)
        sema_up (&proc->sema);
    }
}

int ipc_receive (char *signature)
{
  struct list_elem *e;
  int data;

  /* Iterate over messages in sent mail to find a message with the same signature,
  if found, return the message. */
  e = get_msg (signature);

  if (e != NULL)
    {
      struct message *msg = list_entry (e, struct message, elem);

      list_remove (e);
      data = msg->data;
      free (msg->signature);
      free (msg);
      return data;
    }

  /* If never found a message with this signature, wait for someone to pass a message with this
  signature using a semaphore. */
  struct waiting_process *proc = malloc (sizeof (struct waiting_process));
  proc->signature = malloc (sizeof (char *));
  strlcpy (proc->signature, signature, strlen (signature) + 1);

  sema_init (&proc->sema, 0);

  list_push_back (&waiting_list, &proc->elem);
  /* Wait on semaphore. */
  sema_down (&proc->sema);

  /* A message with signature is put in mail. */
  e = get_msg (signature);

  if (e != NULL)
    {
      struct message *msg = list_entry (e, struct message, elem);

      list_remove (e);
      list_remove (&proc->elem);
      data = msg->data;
      free (msg->signature);
      free (msg);
      free (proc->signature);
      free (proc);
      return data;
    }

  NOT_REACHED ();
}
