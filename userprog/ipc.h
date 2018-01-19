#ifndef USERPROG_IPC_H
#define USERPROG_IPC_H

/* Initializes communication system of IPC. */
void ipc_init (void);
/* Send a message with a signature via IPC. */
void ipc_send (char *signature, int msg);
/* Receive a message with a signature via IPC */
int ipc_receive (char *signature);

#endif /* userprog/ipc.h */
