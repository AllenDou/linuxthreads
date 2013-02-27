/* Linuxthreads - a simple clone()-based implementation of Posix        */
/* threads for Linux.                                                   */
/* Copyright (C) 1996 Xavier Leroy (Xavier.Leroy@inria.fr)              */
/*                                                                      */
/* This program is free software; you can redistribute it and/or        */
/* modify it under the terms of the GNU Library General Public License  */
/* as published by the Free Software Foundation; either version 2       */
/* of the License, or (at your option) any later version.               */
/*                                                                      */
/* This program is distributed in the hope that it will be useful,      */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/* GNU Library General Public License for more details.                 */

/* Internal data structures */

/* Includes */

#include <sys/types.h>
#include <setjmp.h>
#include <signal.h>
#include <gnu-stabs.h>          /* for weak_alias */
#include <linux/mm.h>

#include "pt-machine.h"

/* Arguments passed to thread creation routine */

struct pthread_start_args {
  void * (*start_routine)(void *); /* function to run */
  void * arg;                   /* its argument */
  sigset_t mask;                /* initial signal mask for thread */
  int schedpolicy;              /* initial scheduling policy (if any) */
  struct sched_param schedparam; /* initial scheduling parameters (if any) */
};

#define PTHREAD_START_ARGS_INITIALIZER { NULL, NULL, 0, 0, { 0 } }

/* We keep thread specific data in a special data structure, a two-level
   array.  The top-level array contains pointers to dynamically allocated
   arrays of a certain number of data pointers.  So we can implement a
   sparse array.  Each dynamic second-level array has
	PTHREAD_KEY_2NDLEVEL_SIZE
   entries.  This value shouldn't be too large.  */
#define PTHREAD_KEY_2NDLEVEL_SIZE	32

/* We need to address PTHREAD_KEYS_MAX key with PTHREAD_KEY_2NDLEVEL_SIZE
   keys in each subarray.  */
#define PTHREAD_KEY_1STLEVEL_SIZE \
  ((PTHREAD_KEYS_MAX + PTHREAD_KEY_2NDLEVEL_SIZE - 1) \
   / PTHREAD_KEY_2NDLEVEL_SIZE)

/* The type of thread descriptors */

typedef struct _pthread_descr_struct * pthread_descr;

struct _pthread_descr_struct {
  pthread_descr p_nextlive, p_prevlive;
                                /* Double chaining of active threads */
  pthread_descr p_nextwaiting;  /* Next element in the queue holding the thr */
  pthread_t p_tid;              /* Thread identifier */
  int p_pid;                    /* PID of Unix process */
  int p_priority;               /* Thread priority (== 0 if not realtime) */
  int * p_spinlock;             /* Spinlock for synchronized accesses */
  int p_signal;                 /* last signal received */
  sigjmp_buf * p_signal_jmp;    /* where to siglongjmp on a signal or NULL */
  sigjmp_buf * p_cancel_jmp;    /* where to siglongjmp on a cancel or NULL */
  char p_terminated;            /* true if terminated e.g. by pthread_exit */
  char p_detached;              /* true if detached */
  char p_exited;                /* true if the assoc. process terminated */
  void * p_retval;              /* placeholder for return value */
  int p_retcode;                /* placeholder for return code */
  pthread_descr p_joining;      /* thread joining on that thread or NULL */
  struct _pthread_cleanup_buffer * p_cleanup; /* cleanup functions */
  char p_cancelstate;           /* cancellation state */
  char p_canceltype;            /* cancellation type (deferred/async) */
  char p_canceled;              /* cancellation request pending */
  int p_errno;                  /* error returned by last system call */
  int p_h_errno;                /* error returned by last netdb function */
  struct pthread_start_args p_start_args; /* arguments for thread creation */
  void ** p_specific[PTHREAD_KEY_1STLEVEL_SIZE]; /* thread-specific data */
};

/* The type of thread handles. */

typedef struct pthread_handle_struct * pthread_handle;

struct pthread_handle_struct {
  int h_spinlock;               /* Spinlock for sychronized access */
  pthread_descr h_descr;        /* Thread descriptor or NULL if invalid */
};

/* The type of messages sent to the thread manager thread */

struct pthread_request {
  pthread_descr req_thread;     /* Thread doing the request */
  enum {                        /* Request kind */
    REQ_CREATE, REQ_FREE, REQ_PROCESS_EXIT, REQ_MAIN_THREAD_EXIT
  } req_kind;
  union {                       /* Arguments for request */
    struct {                    /* For REQ_CREATE: */
      const pthread_attr_t * attr; /* thread attributes */
      void * (*fn)(void *);     /*   start function */
      void * arg;               /*   argument to start function */
      sigset_t mask;            /*   signal mask */
    } create;
    struct {                    /* For REQ_FREE: */
      pthread_descr thread;     /*   descriptor of thread to free */
    } free;
    struct {                    /* For REQ_PROCESS_EXIT: */
      int code;                 /*   exit status */
    } exit;
  } req_args;
};

/* Signals used for suspend/restart and for cancellation notification.
   FIXME: shoud use new, unallocated signals. */

#define PTHREAD_SIG_RESTART SIGUSR1
#define PTHREAD_SIG_CANCEL SIGUSR2

/* Global array of thread handles, used for validating a thread id
   and retrieving the corresponding thread descriptor. Also used for
   mapping the available stack segments. */

extern struct pthread_handle_struct __pthread_handles[PTHREAD_THREADS_MAX];

/* Descriptor of the initial thread */

extern struct _pthread_descr_struct __pthread_initial_thread;

/* Descriptor of the manager thread */

extern struct _pthread_descr_struct __pthread_manager_thread;

/* Descriptor of the main thread */

extern pthread_descr __pthread_main_thread;

/* Limit between the stack of the initial thread (above) and the
   stacks of other threads (below). Aligned on a STACK_SIZE boundary.
   Initially 0, meaning that the current thread is (by definition)
   the initial thread. */

extern char * __pthread_initial_thread_bos;

/* File descriptor for sending requests to the thread manager.
   Initially -1, meaning that pthread_initialize must be called. */

extern int __pthread_manager_request;

/* Other end of the pipe for sending requests to the thread manager. */

extern int __pthread_manager_reader;

/* Limits of the thread manager stack. */

extern char * __pthread_manager_thread_bos;
extern char * __pthread_manager_thread_tos;

/* Pending request for a process-wide exit */

extern int __pthread_exit_requested, __pthread_exit_code;

/* Return the handle corresponding to a thread id */

static inline pthread_handle thread_handle(pthread_t id)
{
  return &__pthread_handles[id % PTHREAD_THREADS_MAX];
}

/* Validate a thread handle. Must have acquired h->h_spinlock before. */

static inline int invalid_handle(pthread_handle h, pthread_t id)
{
  return h->h_descr == NULL || h->h_descr->p_tid != id;
}

/* Fill in defaults left unspecified by pt-machine.h.  */

/* The page size we can get from the system.  This should likely not be
   changed by the machine file, but you never know.  */
#ifndef PAGE_SIZE
#define PAGE_SIZE  (sysconf(_SC_PAGE_SIZE))
#endif

/* The max size of the thread stack segments.  If the default
   THREAD_SELF implementation is used, this must be a power of two and
   a multiple of PAGE_SIZE.  */
#ifndef STACK_SIZE
#define STACK_SIZE  (2 * 1024 * 1024)
#endif

/* The initial size of the thread stack.  Must be a multiple of PAGE_SIZE.  */
#ifndef INITIAL_STACK_SIZE
#define INITIAL_STACK_SIZE  (4 * PAGE_SIZE)
#endif

/* Size of the thread manager stack. The "- 32" avoids wasting space
   with some malloc() implementations. */
#ifndef THREAD_MANAGER_STACK_SIZE
#define THREAD_MANAGER_STACK_SIZE  (2 * PAGE_SIZE - 32)
#endif

/* The base of the "array" of thread stacks.  The array will grow down from
   here.  Defaults to the calculated bottom of the initial application
   stack.  */
#ifndef THREAD_STACK_START_ADDRESS
#define THREAD_STACK_START_ADDRESS  __pthread_initial_thread_bos
#endif

/* Get some notion of the current stack.  Need not be exactly the top
   of the stack, just something somewhere in the current frame.  */
#ifndef CURRENT_STACK_FRAME
#define CURRENT_STACK_FRAME  ({ char __csf; &__csf; })
#endif

/* Recover thread descriptor for the current thread */
  
static inline pthread_descr thread_self (void) __attribute__((const));
static inline pthread_descr thread_self (void)
{
#ifdef THREAD_SELF
  THREAD_SELF
#else
  char *sp = CURRENT_STACK_FRAME;
  if (sp >= __pthread_initial_thread_bos)
    return &__pthread_initial_thread;
  else if (sp >= __pthread_manager_thread_bos && sp < __pthread_manager_thread_tos)
    return &__pthread_manager_thread;
  else
    return (pthread_descr)(((unsigned long)sp | (STACK_SIZE-1))+1) - 1;
#endif
}

/* Debugging */

#ifdef DEBUG
#include <assert.h>
#define ASSERT assert
#define MSG __pthread_message
#else
#define ASSERT(x)
#define MSG(msg,arg)
#endif

/* Internal global functions */

void __pthread_destroy_specifics(void);
void __pthread_perform_cleanup(void);
void __pthread_sighandler(int sig);
void __pthread_message(char * fmt, long arg);
int __pthread_manager(void * reqfd);
void __pthread_manager_sighandler(int sig);
void __pthread_reset_main_thread(void);
void __fresetlockfiles(void);

/* System calls not declared in libc 5 */

int __clone(int (*child_function)(void *), void ** child_stack, int flags,
            void * arg);
int __nanosleep(const struct timespec * rqtp, struct timespec * rmtp);
int __sched_yield(void);
int __sched_setparam(pid_t pid, const struct sched_param *param);
int __sched_getparam(pid_t pid, struct sched_param *param);
int __sched_setscheduler(pid_t pid, int policy,
                         const struct sched_param *param);
int __sched_getscheduler(pid_t pid);
int __sched_get_priority_max(int policy);
int __sched_get_priority_min(int policy);
int __sched_rr_get_interval(pid_t pid, struct timespec *interval);
