# **Design Document: Project #02: Userprograms**

## **Argument Passing**

### Data Structures:
- (A1) Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or `enum`.

	None created or changed.

### Algorithms:
- (A2) Briefly describe how you implemented argument parsing. How do you arrange for the elements of   `argv[]` to be in the right order? How do you avoid overflowing the stack page?

    By passing dynamically allocated space for `args` and a counter `argc` after parsing command line into `argv` using `strtok_r` to `setup_stack` after both kernel page and user page are installed.

    `setup_stack` in turn uses `memcpy` to push memory from arguments to stack space before `PHYS_BASE`, pushes pointers to their arguments addresses space after, and finally push a pointer to `argv`, `argc` and a fake return address to 0 to accomodate gcc compiler.

    Overflow in stack page is handled by checking if size of arguments is big enough to overflow, if it is, thread exits immediately with -1 status.

### Rationale:
- (A3) Why does Pintos implement `strtok_r ()` but not `strtok ()`?

    `strtok` saves a static global pointer for reuse in next time function is invoked with NULL as the first parameter, meaning that this pointer gets messed up when working on two string in parallel, unlike in `strtok_r` where a `save_ptr` is passed externally, meaning that it can work on two strings in parallel using these externally passed pointers.

- (A4) In Pintos, the kernel separates commands into a executable name and arguments. In Unix-like systems, the shell does this separation.  Identify at least two advantages of the Unix approach.

    Since the Unix shell identifies as a userprogram, it seems more intuitive that it can separate arguments from desired userprog name/path before passing it to the kernel, which in turn has no use at all in treating userprog's name and arguments as a whole entity; thus, by using the Unix systems approach, the kernel spends less time doing a job that can be done in user space.

    Another reason is that by using the shell, checking for the passed userprog's potential failure can be done before passing arguments to the kernel (for example, checking if the ELF binary exists before passing its name/path), this approach avoids kernel exceptions and failures as much as possible.
---

## **System Calls**

### Data Structures:

- (B1) Copy here the declaration of each new or changed `struct` or `struct` member, global or static variable, `typedef`, or `enum`.

	```c
	/* userprog/process.h */
    struct process
    {
      pid_t pid;                           /* PID used to identify a process. */
      struct list files;                   /* List of files opened by this process. */

      struct list children_processes;      /* List of children processes which are added whenever
                                              this process invokes an 'exec' system call. */
      struct list_elem elem;               /* List element used to add this process in its parent's
                                              children processes list. */
      struct list_elem allelem;            /* List element used to add this process in list of all
                                              processes in the system. */
      struct file *executable;             /* ELF binary file of this process. */
    };

	/*--------------------------------------------------------------*/
	/* userprog/syscall.c */

    /* Global variable used to identify a file descriptor by its ID. */
    int fid;
    /* Lock to protect `fid` variable to avoid race conditions when multiple
       processes try to open a file at the same time. */
    static struct lock fid_lock;

    struct file_elem
    {
      int fid;                              /* Unique ID for this file. */
      void* data;                           /* Actual data of file this element holds. */
      struct list_elem elem;                /* List element used to add this file to list of
                                               process' open files. */
    };

    /* List of function pointers to handlers of 13 supported system calls. */
    static void (*syscall_handlers[SYSCALL_COUNT]) (struct intr_frame *);

    /*--------------------------------------------------------------*/
	/* userprog/syscall.c */

    /* Lock used to be acquired and released externally whenever
       a file system functions is invoked by a system call, support
       is provided by two functions `filesys_acquire_external_lock`,
       `filesys_acquire_release_lock`. */
    static struct lock file_system;

    /*--------------------------------------------------------------*/
	/* userprog/ipc.c */
    /* A module that uses a blocking IPC mechanism to support communication
       between child processes and parent processes. */

    /* Holds data that identifies a message sent to the IPC system using a given unique
       signature. */
    struct message
    {
      char *signature;            /* Signature that differentiates a message from other messages,
                                     (should always be unique). */
      int data;                   /* Data to be sent with the previously mentioned signature. */
      struct list_elem elem;
    };

    /* A process waiting for a message to be sent to the IPC module waits on a semaphore,
       gets awakened whenever the message is sent. */
    struct waiting_process
    {
      char *signature;            /* Signature to receive a message with. */
      struct semaphore sema;      /* Semaphore used to make processes wait on whenever they try to
                                     receive a message that is not yet put in list. */
      struct list_elem elem;
    };
	```

- (B2) Describe how file descriptors are associated with open files. Are file descriptors unique within the entire OS or just within a single process?

	File descriptors are unique whithin the entire OS. This approach avoids storing too much information per process.

### Algorithms:

- (B3) Describe your code for reading and writing user data from the kernel.
 	
	By firstly validating all pointers to be less than the PHYS_BASE addresses, directing access to memory after, if an invalid access (a null-pointer, a pointer to kernel address space or any other invalid memory operation) is performed, a page fault is issued and a page fault handler is called, in which registers are reset and EAX is set to -1 in case of an invalid access in kernel, process is directed to to the next instruction using EIP register.
	
	After ensuring that all pointers are valid, a subsequent call to file system read or write is issued, it is ensured that parallel read or write operations are synchronized using a file system lock, this lock is acquired before reading from or writing into any file in file system and released after the operation.

- (B4) Suppose a system call causes a full page (4,096 bytes) of data to be copied from user space into the kernel. What is the least and the greatest possible number of inspections of the page table (e.g. calls to `pagedir_get_page()`) that might result?  What about for a system call that only copies 2 bytes of data? Is there room for improvement in these numbers, and how much?

    The least number is one, meaning that inspected data are stored exactly in a single page, the pointer returning from `pagdir_get_page` is enough to get the whole stored data with no need to inspect more.

    The largest number is 4096, inspected data are sparsed on a byte-size level across 4096 pages, thus `pagedir_get_page` is called once per byte.

    For 2 bytes of data, the least number is 1: the two bytes are existent in the same page. The largest number is 2: the two bytes are existent in separate pages.

	There is no way to avoid doint two page lookups, so no improvement can be made.


- (B5) Briefly describe your implementation of the "wait" system call and how it interacts with process termination.

	First it is ensured that the given `tid` responds to a child process resident in the parent process' list, this is done by iterating over list of children processes to find the process with the given tid.

	Parent process requests a message from the child's terminating process using the IPC module, meaning that the parent process tries to receive a message with the signature `"exit $child_tid"`, when a child terminates, it sends a message to IPC with the signature `"exit $process_tid"`, the ipc catches this message and wakes up the parent's process waiting on a local semaphore, giving it the message, which is the child's exit status.

	IPC implicitly handles two cases of child termination, if the parent process issues a `wait` system call before the child process exits, then it gives the IPC the signature it waits for to enter the IPC and sleeps using a semaphore, waiting to be woken up whenever child process sends its exit status. The other case is when parent process issues a `wait` system call *after* child process exits, IPC also handles this case by checking first if a message sent by child process is resident, if there is a message, the parent process is given this mesesage without blocking.

- (B6) Any access to user program memory at a user-specified address can fail due to a bad pointer value. Such accesses must cause the process to be terminated. System calls are fraught with such accesses, e.g. a `write` system call requires reading the system call number from the user stack, then each of the 	call's three arguments, then an arbitrary amount of user memory, and any of these can fail at any point. This poses a design and error-handling problem: how do you best avoid obscuring the primary function of code in a morass of error-handling? Furthermore, when an error is detected, how do you ensure that all temporarily allocated resources (locks, buffers, etc.) are freed? In a few paragraphs, describe the strategy or strategies you adopted for managing these issues. Give an example.

	Using code provided in `get_user`, `put_user` functions, every pointer is checked for referencing a valid address, logic is separated in separate validation functions used in `syscall.c`, a process that references an invalid address is killed.

	Before issuing process to `exit`, its allocated resources are freed accordingly, its open files are closed ..etc, after all the procedures are done, a call to `exit` is made with status -1, indicating an error.

### Synchronization:

- (B7) The `exec` system call returns -1 if loading the new executable fails, so it cannot return before the new executable has completed loading. How does your code ensure this? How is the load success/failure status passed back to the thread that calls "exec"?

	Using IPC mechanism provided in `ipc.c` similarily to `wait`, parent sleeps using a semaphore waiting for child process to put a message with the signature `"exec $process_tid"` in IPC buffers, which in turn wakes up parent process giving it the message indicating whether child process loaded successfully or not.

- (B8) Consider parent process P with child process C. How do you ensure proper synchronization and avoid race conditions when P calls wait(C) before C exits?  After C exits? How do you ensure that all resources are freed in each case? How about when P terminates without waiting, before C exits? After C exits? Are there any special cases?

	When P waits for process C, it puts a request in IPC indicating with the signature `"exit $child_pid"`, meaning that it waits for child process to put a message itself in IPC that contains its exiting status, two cases rise, first is when P waits for process C that has not exited yet, if that's the case then it sleeps using a semaphore in IPC, waiting for child process to put its message in IPC with the same signature, which in turn wakes up the parent process, process wakes up, removes itself from list of waiting processes, and gets child's exiting status. The second case is when P waits for C *after* it has exited, IPC checks whether there is a message with the given signature in its mail before making the process block using semaphore, thus when child C exits before process P does, process P doesn't block, it gets child's exiting status immediately.

	When P terminates without waiting, it frees its resources normally, including its children list and dynamically allocated resources, two cases rise, the first is when process P exits before C does, the second is when it exits after C does, in both cases, a message issued from child C is sent to no one, since parent won't receive it, nothing happens in both cases.


### Rationale:

- (B9) Why did you choose to implement access to user memory from the kernel in the way that you did?

	Implemented accessing memory access using `put_user`, `get_user` functions, which utilize CPU's memory management unit, thus doing faster checks for address validity, and because this method tends to be used in real operating systems like Linux.

- (B10) What advantages or disadvantages can you see to your design for file descriptors?
	
	File descriptors are simple and lightweight that do not require a record for file state, can be easily adjusted, duplicated or changed for the whole process, retrieving a file corresponding to an `fid` operates in O(n) time where n is number of opened files for this process.
	
	On the other hand, if a process opens the same file over and over, the file entry gets duplicated and added every time it is opened, these duplicates can be avoided by checking if the required operation needs for a duplicate file entry. Also since `fid` is allocated per entire OS, it can suffer from overflow more than if an `fid` is allocated per process.

- The default tid_t to pid_t mapping is the identity mapping. If you changed it, what advantages are there to your approach?

	They were left unchanged.