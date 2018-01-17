#ifndef USERPROG_IPC_H
#define USERPROG_IPC_H

/* Initializes communication system of IPC. */
void init_ipc ();
/* Send a message with a signature via IPC. */
void send_ipc (char *signature, int msg);
/* Receive a message with a signature via IPC */
int receive_ipc (char *signature);

#endif /* userprog/ipc.h */
