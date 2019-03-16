#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <stdlib.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#include "threads/fixed_point.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif


/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;
/* linked list from list.c contains blocked threads*/
static struct list blocked_list;

/* List of all thread : current + ready_list + blocked threads */   //we added
static struct list all_thread_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);
int load_avg;


static bool
value_priority_more (const struct list_elem *a_, const struct list_elem *b_,
            void *aux UNUSED);


/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */

/* This is 2016 spring cs330 skeleton code */

void
thread_init (void)   //when system boot
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&blocked_list);
  list_init (&all_thread_list);
  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
  load_avg = 0;
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();
  int ready_threads;

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;
  if(thread_mlfqs){
    int64_t tt = timer_ticks();
    if(tt % 4 ==0){
      thread_renew_priority();
    }
    if(strcmp(thread_current()->name, "idle"))   /*increment by 1 unless idle thread running */
      ADD_FP_INT(thread_current()->recent_cpu, 1);
    
    if(tt % 100 == 0)
      thread_renew_recent_cpu();
    
    if(tt % TIMER_FREQ == 0)
    {
      ready_threads = list_size(&ready_list);
      if(strcmp(thread_current()->name, "idle")) //if not idle
      {
        ADD_FP_INT(ready_threads, 1);  //ready_threads++
        printf("ready_threads: %d \n", ready_threads);
      }
      int f59 = INT_TO_FP(59);
      int f60 = INT_TO_FP(60);
      int f1 = INT_TO_FP(1);
      load_avg = ADD_FPS( MUL_FPS(DIV_FPS(f59, f60), load_avg), MUL_FPS(DIV_FPS(f1, f60), ready_threads));
      // load_avg = (59/60) * load_avg + (1/60) * ready_threads;
      //printf("100 ticks OK - load_avg = %d\n", load_avg);
    }
  }
  //TODO: check fp
  
  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Renew priority of all threads: current, ready, blocked thread */
void 
thread_renew_priority(void)
{
  struct thread * t;
  struct list_elem * l;
  size_t i;
  struct thread * curr;
  curr = thread_current();

  int f63 = INT_TO_FP(63);  //PRI_MAX
  
  if(!list_empty(&all_thread_list))
  {
    t = list_entry(all_thread_list.head.next, struct thread, elem);
    for(l = list_begin(&all_thread_list); l!= list_end(&all_thread_list); l = list_next(l))
    {
      t =  list_entry(l, struct thread, elem);
      t->priority = FP_TO_INT_ROUND_NEAR(SUB_FPS(f63, ADD_FP_INT(DIV_FP_INT(t->recent_cpu, 4), (t->nice * 2))));
    }

  // curr->priority = //renew current thread
  //   FP_TO_INT_ROUND_NEAR(SUB_FPS(f63, ADD_FP_INT(DIV_FP_INT(curr->recent_cpu, 4), (curr->nice * 2))));
  //   //FP_TO_INT_ROUND_NEAR(PRI_MAX - (curr->recent_cpu / 4) - (curr->nice * 2));
  // if(!list_empty(&ready_list)){//renew threads in ready list
  //   t = list_entry(ready_list.head.next, struct thread, elem);
  //   for(l = list_begin(&ready_list); l!= list_end(&ready_list); l = list_next(l)){
  //     t =  list_entry(l, struct thread, elem);
  //     t->priority = FP_TO_INT_ROUND_NEAR(SUB_FPS(f63, ADD_FP_INT(DIV_FP_INT(t->recent_cpu, 4), (t->nice * 2))));
  //     //t->priority = FP_TO_INT_ROUND_NEAR(PRI_MAX - (t->recent_cpu / 4) - (t->nice * 2));
  //   }
  // }
  // if(!list_empty(&blocked_list)){//renew threads in blocked list
  //   t = list_entry(blocked_list.head.next, struct thread, elem);
  //   for(l = list_begin(&blocked_list); l!= list_end(&blocked_list); l = list_next(l)){
  //     t =  list_entry(l, struct thread, elem);
  //     t->priority = FP_TO_INT_ROUND_NEAR(SUB_FPS(f63, ADD_FP_INT(DIV_FP_INT(t->recent_cpu, 4), (t->nice * 2))));
  //     //t->priority = FP_TO_INT_ROUND_NEAR(PRI_MAX - (t->recent_cpu / 4) - (t->nice * 2));
  //   }
  // }  
  }
}
//TODO: check fp

/* Renew recent_cpu of all threads: current, ready, blocked thread */
void
thread_renew_recent_cpu(void)
{
  struct thread * t;
  struct list_elem * l;
  size_t i;
  struct thread * curr;
  curr = thread_current();
  int load_avg = thread_get_load_avg();

  if(!list_empty(&all_thread_list))
  {
    t = list_entry(all_thread_list.head.next, struct thread, elem);
    for(l = list_begin(&all_thread_list); l!= list_end(&all_thread_list); l = list_next(l))
    {
      t =  list_entry(l, struct thread, elem);
      //t->recent_cpu = (2*load_avg)/(2*load_avg + 1) * t->recent_cpu + t->nice;
      ADD_FPS(MUL_FPS(DIV_FPS(MUL_FP_INT(load_avg, 2), ADD_FP_INT(MUL_FP_INT(load_avg, 2), 1)), t->recent_cpu), t->nice);
    }
  }

  // curr->recent_cpu = //renew current thread
  // (2*load_avg)/(2*load_avg + 1) * curr->recent_cpu + curr->nice;
  // ADD_FPS(MUL_FPS(DIV_FPS(MUL_FP_INT(load_avg, 2), ADD_FP_INT(MUL_FP_INT(load_avg, 2), 1)), curr->recent_cpu), curr->nice);
  // //recent_cpu = (2*load_avg)/(2*load_avg + 1) * recent_cpu + nice
  // if(!list_empty(&ready_list)){//renew threads in ready list
  //   t = list_entry(ready_list.head.next, struct thread, elem);
  //   for(l = list_begin(&ready_list); l!= list_end(&ready_list); l = list_next(l)){
  //     t =  list_entry(l, struct thread, elem);
  //     //t->recent_cpu = (2*load_avg)/(2*load_avg + 1) * t->recent_cpu + t->nice;
  //     ADD_FPS(MUL_FPS(DIV_FPS(MUL_FP_INT(load_avg, 2), ADD_FP_INT(MUL_FP_INT(load_avg, 2), 1)), t->recent_cpu), t->nice);
  //   }
  // }
  // if(!list_empty(&blocked_list)){//renew threads in blocked list
  //   t = list_entry(blocked_list.head.next, struct thread, elem);
  //   for(l = list_begin(&blocked_list); l!= list_end(&blocked_list); l = list_next(l)){
  //     t =  list_entry(l, struct thread, elem);
  //     //t->recent_cpu = (2*load_avg)/(2*load_avg + 1) * t->recent_cpu + t->nice;
  //     ADD_FPS(MUL_FPS(DIV_FPS(MUL_FP_INT(load_avg, 2), ADD_FP_INT(MUL_FP_INT(load_avg, 2), 1)), t->recent_cpu), t->nice);
  //   }
  // }  
}
//TODO: check fp

void
thread_push_priority(struct thread * t) /*we added*/
{
  list_insert_ordered(&ready_list, &(t->elem), value_priority_more, NULL);

  if(thread_current()->priority < list_entry(list_front(&ready_list), struct thread, elem)->priority)
  {
    thread_yield();
  }
};

void
thread_compare_curr_ready(void)
{
  if(!(list_empty(&ready_list)) && thread_current()->priority < list_entry(list_front(&ready_list), struct thread, elem)->priority)
  {
    thread_yield();
  }
};

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;

  /* Add to run queue. */
  thread_unblock (t);

  thread_compare_curr_ready(); /*we added*/

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) //TODO: if the block is in the ready_list, popit..
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Returns true if ticks of A is less than ticks of B, false
   otherwise. */ /*we addend*/
static bool
value_tick_less (const struct list_elem *a_, const struct list_elem *b_,
            void *aux UNUSED) 
{
  const struct tick_elem *a = list_entry (a_, struct tick_elem, elem);
  const struct tick_elem *b = list_entry (b_, struct tick_elem, elem);
  return a->ticks < b->ticks;
}

/* Returns true if priority of A is more than that of B, false
   otherwise. */ /*we addend*/
static bool
value_priority_more (const struct list_elem *a_, const struct list_elem *b_,
            void *aux UNUSED) 
{
  const struct thread *a = list_entry (a_, struct thread, elem);
  const struct thread *b = list_entry (b_, struct thread, elem);
  return a->priority > b->priority;
}



/* put blocked_thread with wakeup tick in the linked list*/
void
thread_block_timered (int64_t ticks, int64_t ticks_tosleep) /*we added*/  
{
  struct list_elem e;
  struct tick_elem *te = (struct tick_elem *)malloc(sizeof(struct tick_elem)); //TODO:Free it when destroy
  te->elem = e;
  te->ticks = ticks + ticks_tosleep;
  te->t = thread_current ();
  list_insert_ordered(&blocked_list, &(te->elem), value_tick_less, NULL);
  enum intr_level old_level;
  old_level = intr_disable ();
  thread_block ();
  intr_set_level (old_level);
}


void
thread_wakeup_blocked (int64_t ticks) /*we added*/
{
  if(list_empty(&blocked_list))
    return;  //nothing to declaire
  struct tick_elem *te = list_entry(list_front(&blocked_list), 
    struct tick_elem, elem);  
  enum intr_level old_level;
  if (te->ticks == ticks) 
  {
    struct thread * t = te->t;
    list_pop_front(&blocked_list);
    thread_unblock(t);
    thread_wakeup_blocked (ticks);
  }
//  compare_curr_ready();
}
/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  list_insert_ordered(&ready_list, &(t->elem), value_priority_more, NULL);
  //list_push_back (&ready_list, &t->elem);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Just set our status to dying and schedule another process.
     We will be destroyed during the call to schedule_tail(). */
  intr_disable ();
  list_remove(&thread_current()->elem_all);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *curr = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (curr != idle_thread) 
    list_insert_ordered(&ready_list, &(curr->elem), value_priority_more, NULL);
    //list_push_back (&ready_list, &curr->elem);
  curr->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  if(thread_mlfqs == true)
    return thread_current()->priority;
  if(!thread_current()->donation_flag)  //no donation
    thread_current ()->priority = new_priority;
  else if(new_priority > thread_current()->priority)
  {
    thread_current()->donation_flag = false;
    thread_current()->priority = new_priority;
  }
  
  thread_current ()->original_priority = new_priority;
  thread_compare_curr_ready(); //we added
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) 
{
  struct thread * curr;
  curr = thread_current();
  curr->nice = nice;
  int f63 = INT_TO_FP(63);
  //curr->priority = FP_TO_INT_ROUND_NEAR(PRI_MAX - (curr->recent_cpu / 4) - (curr->nice * 2));
  curr->priority = FP_TO_INT_ROUND_NEAR(SUB_FPS(f63, ADD_FP_INT(DIV_FP_INT(curr->recent_cpu, 4), (curr->nice * 2))));
} 
//TODO: check fp

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  return thread_current()->nice;
}

/* Returns 100 times the system load average. */
/*TODO: comment */
int
thread_get_load_avg (void) 
{
  //printf("********avg: %d", load_avg);
  return FP_TO_INT_ROUND_NEAR (load_avg * 100);
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  return FP_TO_INT_ROUND_NEAR(MUL_FP_INT(thread_current()->recent_cpu, 100));
  //return FP_TO_INT_ROUND_NEAR (thread_current()->recent_cpu * 100);
}
//TODO: check fp

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);
                    
  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Since `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;

  t->nice = 0;
  t->recent_cpu = 0;
  if(thread_mlfqs == true)
  {
    int f63 = INT_TO_FP(63);  //PRI_MAX
    priority = FP_TO_INT_ROUND_NEAR(SUB_FP_INT(f63, ADD_FP_INT(DIV_FP_INT(t->recent_cpu, 4), (t->nice * 2))));   //TODO: check
    priority = FP_TO_INT_ROUND_NEAR(PRI_MAX - (t->recent_cpu / 4) - (t->nice * 2));
  }
  else // round_robin
    t->priority = priority;

  t->magic = THREAD_MAGIC;
  list_init(&t->lock_list);
  t->original_priority = priority;
  t->need_lock = NULL;

  if(strcmp(t->name, "idle"))  //not an idle
    list_push_back(&all_thread_list, &t->elem_all);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}
/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list)){
    if(!list_empty(&blocked_list)){//non in ready_list
      int64_t ticks = timer_ticks();
      int64_t front_ticks = list_entry(list_front(&blocked_list), struct tick_elem, elem)->ticks;
      

      if(front_ticks <= ticks){//existing thread in blocked_list
        while(front_ticks <= ticks){
          thread_wakeup_blocked(front_ticks);
          front_ticks = list_entry(list_front(&blocked_list), struct tick_elem, elem)->ticks;
        }
        return next_thread_to_run();
      }
    }
    return idle_thread;//non in blocked_list or front is not ready to wakeup
  }
  else// existing thread in ready_list
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
schedule_tail (struct thread *prev) 
{
  struct thread *curr = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  curr->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != curr);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.
   
   It's not safe to call printf() until schedule_tail() has
   completed. */
static void
schedule (void) 
{
  struct thread *curr = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;
  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (curr->status != THREAD_RUNNING);
  ASSERT (is_thread (next));
  if (curr != next)
    prev = switch_threads (curr, next);
  schedule_tail (prev); 
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);