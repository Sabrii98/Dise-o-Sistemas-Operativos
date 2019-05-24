#include <stdio.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>
#include "mythread.h"
#include "interrupt.h"
#include "queue.h"

TCB* scheduler();
void activator();
void timer_interrupt(int sig);
void disk_interrupt(int sig);

/* Declaramos la cola que se va a utilizar */
struct queue* queue;

/* Declaramos el anterior thread y su id */
static TCB* last;
static int last_id = 0;

/* Array of state thread control blocks: the process allows a maximum of N threads */
static TCB t_state[N];

/* Current running thread */
static TCB* running;
static int current = 0;

/* Variable indicating if the library is initialized (init == 1) or not (init == 0) */
static int init = 0;

/* Thread control block for the idle thread */
static TCB idle;
static void idle_function(){
  while(1);
}

/* Initialize the thread library */
void init_mythreadlib() {
  int i;
  /* Inicializamos la cola */
  queue = queue_new();
  /* Create context for the idle thread */
  if(getcontext(&idle.run_env) == -1) {
    perror("*** ERROR: getcontext in init_thread_lib\n");
    exit(-1);
  }
  idle.state = IDLE;
  idle.priority = SYSTEM;
  idle.function = idle_function;
  idle.run_env.uc_stack.ss_sp = (void *)(malloc(STACKSIZE));
  idle.tid = -1;
  if(idle.run_env.uc_stack.ss_sp == NULL) {
    printf("*** ERROR: thread failed to get stack space\n");
    exit(-1);
  }
  idle.run_env.uc_stack.ss_size = STACKSIZE;
  idle.run_env.uc_stack.ss_flags = 0;
  idle.ticks = QUANTUM_TICKS;
  makecontext(&idle.run_env, idle_function, 1);
  t_state[0].state = INIT;
  t_state[0].priority = LOW_PRIORITY;
  t_state[0].ticks = QUANTUM_TICKS;
  if(getcontext(&t_state[0].run_env) == -1) {
    perror("*** ERROR: getcontext in init_thread_lib\n");
    exit(5);
  }
  /* Comprobamos si el QUANTUM_TICKS no tiene errores (si es menor que 0) */
  if(QUANTUM_TICKS < 0) {
    printf("ERROR: the variable QUANTUM TICKS must be greater than zero\n");
    exit(-1);
  }
  for(i = 1; i < N; i++) {
    t_state[i].state = FREE;
  }
  t_state[0].tid = 0;
  running = &t_state[0];
  /* Initialize disk and clock interrupts */
  init_disk_interrupt();
  init_interrupt();
}

/* Create and intialize a new thread with body fun_addr and one integer argument */
int mythread_create(void (*fun_addr)(), int priority) {
  int i;
  if(!init) {
    init_mythreadlib();
    init = 1;
  }
  for(i = 0; i < N; i++) {
    if(t_state[i].state == FREE) {
      break;
    }
  }
  if(i == N) {
    return(-1);
  }
  if(getcontext(&t_state[i].run_env) == -1) {
    perror("*** ERROR: getcontext in my_thread_create\n");
    exit(-1);
  }
  t_state[i].state = INIT;
  t_state[i].priority = priority;
  t_state[i].ticks = QUANTUM_TICKS;
  t_state[i].function = fun_addr;
  t_state[i].run_env.uc_stack.ss_sp = (void *)(malloc(STACKSIZE));
  if(t_state[i].run_env.uc_stack.ss_sp == NULL) {
    printf("*** ERROR: thread failed to get stack space\n");
    exit(-1);
  }
  t_state[i].tid = i;
  t_state[i].run_env.uc_stack.ss_size = STACKSIZE;
  t_state[i].run_env.uc_stack.ss_flags = 0;
  makecontext(&t_state[i].run_env, fun_addr, 1);
  disable_interrupt();
  /* Encolamos el proceso recién creado */
  enqueue(queue, &t_state[i]);
  enable_interrupt();
  return i;
}

/* Read disk syscall */
int read_disk() {
  return 1;
}

/* Disk interrupt */
void disk_interrupt(int sig) {

}

/* Free terminated thread and exits */
void mythread_exit() {
  int tid = mythread_gettid();
  printf("*** THREAD %d FINISHED\n", tid);
  t_state[tid].state = FREE;
	free(t_state[tid].run_env.uc_stack.ss_sp);
  /* Llamamos al scheduler para que nos devuelva el siguiente proceso que hay que ejecutar y luego a la función activator para ejecutar ese proceso */
  TCB* new_thread = scheduler();
  printf("*** THREAD %d TERMINATED: SETCONTEXT OF %d\n", tid, new_thread->tid);
  activator(new_thread);
}

/* Sets the priority of the calling thread */
void mythread_setpriority(int priority) {
  int tid = mythread_gettid();
  t_state[tid].priority = priority;
}

/* Returns the priority of the calling thread */
int mythread_getpriority(int priority) {
  int tid = mythread_gettid();
  return t_state[tid].priority;
}

/* Get the current thread id */
int mythread_gettid() {
  if(!init) {
    init_mythreadlib();
    init=1;
  }
  return current;
}

/* Scheduler */
TCB* scheduler() {
  disable_interrupt();
  /* Desencolamos el siguiente proceso que se encuentra en la cola */
	if(queue_empty(queue) == 0) {
		enable_interrupt();
    /* Actualizamos el anterior thread y su id */
		last = running;
		last_id = current;
    TCB* aux_thread;
		disable_interrupt();
		aux_thread = dequeue(queue);
		enable_interrupt();
    /* Actualizamos también el thread running y su id */
		running = aux_thread;
		current = aux_thread->tid;
		return aux_thread;
	}
  enable_interrupt();
	printf("*** FINISH\n");
	exit(1);
}

/* Timer interrupt */
void timer_interrupt(int sig) {
  int i = mythread_gettid();
  /* Reducimos el número de ticks que le quedan al proceso que se está ejecutando para terminar */
  t_state[i].ticks--;
  /* Cambiamos al siguiente proceso cuando se termina el tiempo */
  if(t_state[i].ticks == 0) {
    /* Volvemos a ponerle el QUANTUM_TICKS como valor de ticks */
    t_state[i].ticks = QUANTUM_TICKS;
    disable_interrupt();
    enqueue(queue, &t_state[i]);
    enable_interrupt();
    TCB* new_thread = scheduler();
    activator(new_thread);
  }
}

/* Activator */
void activator(TCB* next) {
  /* Comprobamos que el thread que está haciendo el activator no sea el thread last */
  if(last != next) {
    /* Aplicamos el setcontext */
    if(last->state == FREE) {
      if(setcontext(&(next->run_env)) == -1) {
        perror("*** ERROR: error in command setcontext\n");
        exit(-1);
      }
    }
    /* Realizamos un cambio de contexto entre dos threads que aún no han finalizado */
    printf("*** SWAPCONTEXT FROM %d TO %d\n", last->tid, next->tid);
    if(swapcontext(&(last->run_env), &(next->run_env)) == -1) {
      perror("*** ERROR: error in command swapcontext\n");
      exit(-1);
    }
  }
}
