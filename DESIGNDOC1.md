# **Design Document: Project #01: Threads**

## **Alarm Clock**

### Data Structures:
- (A1)

	```c
	/* threads/thread.h */
	struct thread
	{
	    /* Unchanged struct members owned by thread.c */

	    /* Additional struct member identifying ticks till thread wakes from sleep,
	    starting from OS boot time, this member is set by timer_sleep () and checked
	    by timer interrupts to decide whether to wake the thread or let it continue
	    sleeping. */
	    int64_t sleep_ticks;        /* Ticks till thread unblocks from sleep
					   starting from OS boot time. */
	    /* Rest of struct members. */
	};
	/*----------------------------------------------------------------------------*/
	/* devices/timer.c */

	/* Stores all sleeping threads at the moment, ordered by 'sleep_ticks';
	meaning that the thread that sleeps less is always in the beginning of the list. */
	struct list thread_sleep_list;
	```

### Algorithms:
- (A2) Briefly describe what happens in a call to timer_sleep(), including the effects of the timer interrupt handler.

    When `timer_sleep ()` is called, thread's `sleep_ticks` is set to be the number of ticks for thread to sleep until it wakes up starting from OS boot time, thread gets added to the the ordered sleep list and blocks immediately.

	With each `timer_interrupt ()`, number of ticks is incremented, the function loops through sleeping threads to unblock every thread that finishes its sleeping duration, it's worth noting that the loop breaks immediately whenever it reaches a thread that has not finished its sleeping duration as per list ordering, the function checks if any of the newly unblocked threads can preempt the currently running thread, calling `intr_yield_on_return ()` accordingly.

- (A3) What steps are taken to minimize the amount of time spent in the timer interrupt handler?

	When a thread calls `timer_sleep ()`, the function spends as much time as needed to insert the sleeping thread by order of sleep ticks in the list, to avoid latency as much as possible in the timer interrupt, thus whenever a timer interrupt occurs, the loop iterates for a number of times exactly the number of threads to be unblocked, as it breaks whenever it finds a thread that has not finished sleeping yet, utilizing the list ordering by sleep ticks, decreasing the latency of timer interrupt as much as possible.


### Synchronization:
- (A4) How are race conditions avoided when multiple threads call timer_sleep() simultaneously?

	The function disables interrupt temporarily before adding the thread calling `timer_sleep ()` into the list to assure that no thread can preempt while the thread gets added to the list, messing with the list invariant.

- (A5) How are race conditions avoided when a timer interrupt occurs during a call to timer_sleep()?

	Timer interrupts can never preempt `timer_sleep ()` as it temporarily disables external interrupts just before adding the thread to the list of sleeping threads.


### Rationale:
- (A6) Why did you choose this design?  In what ways is it superior to another design you considered?

	This design was minimal and the usage of ordered list for sleeping thread marginally decreased latency in timer interrupts. Other design considerations included usage of an unordered list for sleeping threads but this meant that timer interrupt would have to iterate through all sleeping threads every single time, so it was decided that the overhead in insertion to the list would compensate for the latency in timer interrupts. Another design decision was not to store number of sleep ticks the thread passes as a parameter to `thread_sleep ()` but to store number of ticks it takes since OS boot time, as this meant there will be no need to decrement number of sleeping ticks for every sleeping thread every timer interrupt, a simple check that ensures that number of sleeping ticks > system ticks is enough.

---

## **Priority Scheduling**

### Data Structures:

- (B1)

	```c
	/* threads/thread.h */
	struct thread
	{
	    /* Unchanged struct members owned by thread.c */

	    int default_priority;               /* Default priority of thread; which strores the original
						   priroity of the thread without donations. */
	    struct list locks;                  /* List of all locks held by thread. locks are added to this
						   list when they are first acquired and removed when they are
						   released. */
	    struct lock *lock_waiting;          /* Poniter to the lock which thread is waiting on. */

	    /* Rest of the struct members. */
	};
	/*--------------------------------------------------------------*/
	/* threads/synch.h */

	struct lock
	{
		int priority;               /* Highest priority between the thread(s)
					   trying to acquire or acquiring this lock. */
	    struct list_elem elem;      /* List element for locks list. */
	};
	```

- (B2) 

	When a thread tries to acquire a lock and succeeds to acquire it as there are no other threads holding it yet, this lock gets added to the list of locks acquired by this thread `struct list locks`, if it fails to acquire the thread it waits on a semaphore, and its priority is donated to the thread currently holding the lock if it has a smaller priority, the lock `priority` member is also updated as to correspond to the highest priority of threads trying to acquire the lock, this means that a thread holding multiple locks should have a priority that corresponds to the maximum priority of locks it holds. Donation is aggregated to handle nested donations: the thread waiting on a lock donates to the thread holding the lock, the latter thread in turn donates to the thread holding the lock it waits on and so on...

	Nested donations example:

                                                                              [acquire(b)]                           [release(b)]
                                                                                   ↓                                      ↓
		Thread H (P=33) |──────────────────────────────────────────────────|░░░░░░░|──────────────────────────────────|░░░|░░░|─────────────────────|
		                                  [acquire(b)]   [acquire(a)]      ↑       ¦                  [release(a)]    ¦       ¦
		                                       ↓              ↓         [create]   ¦                       ↓          ¦       ↓
		Thread M (P=32) |──────────────|░░░░░░░|░░░░(P=32)░░░░|────────────¦───────¦────────────|░░(P=33)░░|░░(P=33)░░|───────|░░(P=32)░░|──────────|
				   		 ↑                      ¦            ¦       ¦            ↑                     ↑                  ¦
					      [create]               [donate]        ¦     [donate]       ¦                 [release(b)]           ¦
						 ¦                      ↓            ¦       ↓            ¦                                        ↓
		Thread L (P=31) |░░░░(P=31)░░░░|──────────────────────|░░░(P=32)░░░|───────|░░░(P=33)░░░|────────────────────────────────────────|░░(p=31)░░|
		                ↑
		            [acquire(a)]


### Algorithms:

- (B3) How do you ensure that the highest priority thread waiting for a lock, semaphore, or condition variable wakes up first?

	Every list storing threads waiting on each lock, semaphore or condition variable is ordered by priority of threads in a descending order, when one of the synchronization primitives signals, it pops the thread with the highest priority from the front. A utility function `list_modify_ordered` for lists was added in *list.h* to keep the invariant whenever a thread's priority changes. This function rearranges thread's `list_elem` to keep the list invariant in O(n), instead of sorting each time which would take O(n logn).

- (B4) Describe the sequence of events when a call to lock_acquire() causes a priority donation.  How is nested donation handled?

	Thread A (P=33) tries to acquire a lock that thread B (P=32) holds; thread A's priority > thread B's priority, thus it calls `donate ()` and donates its priority to thread B. which sets priority to thread B, donation is iteratively aggregated to handle nested donations; thread B waits on some lock and donates its new priority to the lock holder and so on. After completion of priority donations, thread A sets itself to be waiting on the lock, and downs the semaphore causing itself to block until lock is released by holder with it being the thread waiting on the semaphore with the highest priority (first in waiters list). After getting unblocked, A adds the lock to its own list of acquired locks and sets `lock_waiting` to *NULL* and sets itself as holder of this lock.

- (B5) Describe the sequence of events when lock_release() is called on a lock that a higher-priority thread is waiting for.

	Thread A releases lock which thread B waits on: thread A sets `lock_holder` to NULL, and removes it from list of locks acquired by thread A, setting `lock_priority` to the highest priority of threads waiting on the lock (assuming it's thread B). Thread A 'ups' the lock's *semaphore* resulting in unblocking thread B and adding it to threads ready queue, but since thread A still has a greater priority or equal to thread B, thread B cannot preempt thread A, resulting in thread A resuming `lock_release ()`, noticing that thread A has not given away its donation yet, thread A donates itself its next priority to have, which is the priority of the highest priority of locks acquired by A, or A's original (default) priority.


### Synchronization:

- (B6) Describe a potential race in thread_set_priority() and explain how your implementation avoids it.  Can you use a lock to avoid this race?

	A race condition could occur if one thread A is interrupted by another thread B setting thread A's priority (by donation for instance) while thread A itself is trying to set its priority calling `thread_set_priority ()`. For example, if thread B has just set thread A's priority but thread A tries to reset its own priority before thread B is done, the ready list will possibly end up being messed up because the list will be sorted with an unexpected value for the priority of B.
	The easy way to avoid this would be to disable interrupts before calling `thread_set_priority ()` and re-enable them afterwards.

	We can use a lock to avoid this race by forcing threads to acquire a lock to the thread priority before setting it and only relinquishing it after all of the priority setting is complete, but it would be of a greater overhead than merely disabling interrupts as `lock_acquire ()` itself must disable interrupts for sometime to perform priority donations, so it seemed to be intuitive to disable interrupts while setting priority.


### Rationale:

- (B7) Why did you choose this design?  In what ways is it superior to another design you considered?

	Implementation does not limit nesting depth of priority donation to any amount, without the use of of a list of all donating threads for every thread. As usage of a list containing locks acquired by thread with setting lock's `priority` member to correspond to the thread with the highest priority waiting seemed to do the trick.

---

## **Advanced Scheduler**

### Data Structures:

- (C1) 

	```c
	/* threads/fixed-point.h */

	/* Defnition of a fixed float in a 32-bit integer */
	typedef int32_t fixed_float;
	/*----------------------------------------------------------------------------*/
	/* threads/thread.h */
	struct thread
	{
	    /* Unchanged struct members owned by thread.c */

	    int nice;                   /* Thread's nice value calculated by mlfqs. */
	    fixed_float recent_cpu;	 /* Thread's recent_cpu value calculated by mlfqs. */

	    /* Rest of struct members. */
	};
	/*----------------------------------------------------------------------------*/
	/* threads/thread.c */

	/* Estimates the average number of threads ready to run over the past minute. */
	fixed_float load_avg;
	```

### Algorithms:

- (C2) Suppose threads A, B, and C have nice values 0, 1, and 2.  Each has a recent_cpu value of 0.  Fill in the table below showing the scheduling decision and the priority and recent_cpu values for each thread after each given number of timer ticks:

	|  Timer ticks | recent_cpu (A B C) | priority (A B C) | Thread to run |
	|--------------|:------------------:|:----------------:|---------------|
	| 0  | 0 0 0   | 63 61 59 | A |
	| 4  | 4 0 0   | 62 61 59 | A |
	| 8  | 8 0 0   | 61 61 59 | B |
	| 12 | 8 4 0   | 61 60 59 | A |
	| 16 | 12 4 0  | 60 60 59 | B |
	| 20 | 12 8 0  | 60 59 59 | A |
	| 24 | 16 8 0  | 59 59 59 | C |
	| 28 | 16 8 4  | 59 59 58 | B |
	| 32 | 16 12 4 | 59 58 58 | A |
	| 36 | 20 12 4 | 58 58 58 | C |

- (C3) Did any ambiguities in the scheduler specification make values in the table uncertain?  If so, what rule did you use to resolve them?  Does this match the behavior of your scheduler?

	If the running thread has the same priority as some thread in the ready queue,
	the scheduler will take the one in the ready queue and then in the next
	time slice will do the same as round-robin. Yes this match with the scheduler as the highest priority one still the one in the running state but this is done to deliver more responsive system.

- (C4) How is the way you divided the cost of scheduling between code inside and outside interrupt context likely to affect performance?

	Most of the calculations for `recent_cpu` and `priority` are done within timer interrupt every fixed number of ticks. Redundancy of calculations in `thread_tick ()` was managed to be cut down by updating priority only for the currently running thread every 4 ticks, and for all threads every 1 sec.

### Rationale:

- (C5) Briefly critique your design, pointing out advantages and disadvantages in your design choices.  If you were to have extra time to work on this part of the project, how might you choose to refine or improve your design?

	Implementation has some advantages in context of design simplicity, as it offers a relatively small thread struct size, the added functionality for `list.h`, specifically `list_modify_ordered ()` managed to cut down the need to perform O(n log n) sortings on ready queue every time a new thread is scheduled, redundancies in calculations were avoided as much as possible by updating priority only for the currently running thread every 4 ticks.

	As for disadvantages, usage of linked lists greatly affected performance, as ordered insertions and modifications running in O(n) complexity are frequently used in code, one suggestion includes usage of a priority queue (Min/Max heaps per se) that supports insertion and modification in O(log n) time with some workaround to ensure stability, and eliminating the need to sort completely. Finally, considerations to refine the design may include detection of overflow in fixed-point operations.

- (C6) The assignment explains arithmetic for fixed-point math in detail, but it leaves it open to you to implement it.  Why did you decide to implement it the way you did?  If you created an abstraction layer for fixed-point math, that is, an abstract data type and/or a set of functions or macros to manipulate fixed-point numbers, why did you do so?  If not, why not?

	'fixed-point.h' was implemented completely by using macros that support fixed-point arithmetic operations, as they are usually faster as they do not need a function call or an activation record, also it allows the compiler to optimize the equations. Real numbers throughout implementation of the scheduler like those used in `recent_cpu` and `load_avg` were 'simulated' using fixed-point representation rather than floating-point arithemtic representation which is marginally slower, besides that it is not implemented in the kernel.
