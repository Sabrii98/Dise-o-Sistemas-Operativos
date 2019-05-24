#include <stdio.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>
#include "interrupt.h"

#define N 10
#define FREE 0
#define INIT 1
#define WAITING 2
#define IDLE 3

#define STACKSIZE 10000
#define QUANTUM_TICKS 40

#define LOW_PRIORITY 0
#define HIGH_PRIORITY 1
#define SYSTEM 2
/* Structure containing thread state */
typedef struct tcb {
  int state;
  int tid;
  int priority;
  int ticks;
  void (*function)(int);
  ucontext_t run_env;
} TCB;

int mythread_create (void (*fun_addr)(), int priority);
void mythread_setpriority(int priority);
int mythread_getpriority();
void mythread_exit();
int mythread_gettid();
int read_disk();

static inline int data_in_page_cache() { return rand() & 0x01; }
