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

/* Declaramos las colas que se van a utilizar: una de alta prioridad, otra de baja prioridad y otra de threads en espera */
struct queue* queue_high;
struct queue* queue_low;
struct queue* queue_wait;

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
static void idle_function() {
  while(1);
}

/* Initialize the thread library */
void init_mythreadlib() {
  int i;
  /* Inicializamos las colas de alta prioridad, de baja prioridad y de espera */
  queue_high = queue_new();
  queue_low = queue_new();
  queue_wait = queue_new();
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
    printf("*** ERROR: the variable QUANTUM TICKS must be greater than zero\n");
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
  /* Controlamos en el mythread_create (después de hacer el makecontext necesario para realizar activator) cuando entra un hilo de alta prioridad y se está ejecutando mientras un hilo de baja prioridad */
  if(priority == HIGH_PRIORITY && running->priority == LOW_PRIORITY) {
    /* Restablecemos el valor del thread de ticks a QUANTUM_TICKS */
    running->ticks = QUANTUM_TICKS;
    disable_interrupt();
    /* Encolamos el thread running en la cola de baja prioridad */
    enqueue(queue_low, running);
    enable_interrupt();
    /* Actualizamos el anterior thread y su id */
    last = running;
    last_id = current;
    /* Actualizamos también el thread running y su id */
    running = &t_state[i];
    current = i;
    /* Cambiamos de contexto */
    activator(running);
  }
  /* Controlamos también en el mythread_create cuando entra un hilo de alta prioridad y se está ejecutando mientras un hilo de alta prioridad */
  else if(priority == HIGH_PRIORITY && running->priority == HIGH_PRIORITY) {
    disable_interrupt();
    /* Encolamos el proceso recién creado en la cola de alta prioridad */
    enqueue(queue_high, &t_state[i]);
    enable_interrupt();
  }
  /* Controlamos también en el my_thread_create cuando entra un hilo de baja prioridad y se está ejecutando mientras un hilo de alta prioridad o un hilo de baja prioridad */
  else if(priority == LOW_PRIORITY && (running->priority == LOW_PRIORITY || running->priority == HIGH_PRIORITY)) {
    disable_interrupt();
    /* Encolamos el proceso recién creado en la cola de baja prioridad */
    enqueue(queue_low, &t_state[i]);
    enable_interrupt();
  }
  return i;
}

/* Read disk syscall */
int read_disk() {
  /* Liberamos la CPU si y sólo si los datos solicitados no están en la caché de páginas */
  if(data_in_page_cache() != 0) {
  	disable_interrupt();
  	enqueue(queue_wait, running);
  	enable_interrupt();
    printf("*** THREAD %i READ FROM DISK\n", running->tid);
    /* Creamos el thread new_thread que será el primer proceso de mayor prioridad que esté listo */
    TCB* new_thread = scheduler();
    /* Activamos el thread new_thread */
    activator(new_thread);
  }
  return 1;
}

/* Disk interrupt */
void disk_interrupt(int sig) {
	disable_interrupt();
	if(queue_empty(queue_wait) == 0) {
    enable_interrupt();
    /* Creamos el thread first_thread que será el primer hilo en la cola de espera */
    TCB* first_thread;
    disable_interrupt();
    first_thread = dequeue(queue_wait);
    enable_interrupt();
    printf("*** THREAD %d READY\n", first_thread->tid);
    if(first_thread->priority == HIGH_PRIORITY) {
      disable_interrupt();
      /* Encolamos el thread first_thread en la cola de alta prioridad */
      enqueue(queue_high, first_thread);
      enable_interrupt();
    }
    else if(first_thread->priority == LOW_PRIORITY) {
      disable_interrupt();
      /* Encolamos el thread first_thread en la cola de baja prioridad */
      enqueue(queue_low, first_thread);
      enable_interrupt();
    }
  }
  else {
    enable_interrupt();
  }
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
    init = 1;
  }
  return current;
}

/* Scheduler */
TCB* scheduler() {
	disable_interrupt();
  /* Desencolamos el siguiente proceso de alta prioridad que se encuentra en la cola */
	if(queue_empty(queue_high) == 0) {
		enable_interrupt();
    /* Actualizamos el anterior thread y su id */
		last = running;
		last_id = current;
    TCB* aux_thread;
		disable_interrupt();
		aux_thread = dequeue(queue_high);
		enable_interrupt();
    /* Actualizamos también el thread running y su id */
		running = aux_thread;
		current = aux_thread->tid;
		return aux_thread;
	}
  enable_interrupt();
  disable_interrupt();
  /* Desencolamos el siguiente proceso de baja prioridad que se encuentra en la cola */
	if(queue_empty(queue_low) == 0) {
		enable_interrupt();
    /* Actualizamos el anterior thread y su id */
		last = running;
		last_id = current;
    TCB* aux_thread;
		disable_interrupt();
		aux_thread = dequeue(queue_low);
		enable_interrupt();
    /* Actualizamos también el thread running y su id */
    running = aux_thread;
		current = aux_thread->tid;
		return aux_thread;
	}
  enable_interrupt();
  disable_interrupt();
  /* Desencolamos el siguiente proceso que se encuentra en la cola de threads en espera */
	if(queue_empty(queue_wait) == 0) {
		enable_interrupt();
    /* Actualizamos el anterior thread y su id */
		last = running;
		last_id = current;
    /* Actualizamos también el thread running y su id, que será el thread idle y su id siempre será -1 */
		running = &idle;
		current = -1;
		return running;
	}
	printf("*** FINISH\n");
	exit(1);
}

/* Timer interrupt */
void timer_interrupt(int sig) {
  int i = mythread_gettid();
  /* Comprobamos primero que el id sea -1, pues en este caso el thread será idle y ejecutará un bucle infinito de tal forma que el planificador consulte si existe algún thread listo para ejecutar */
  if(i == -1) {
    disable_interrupt();
    /* Comprobamos que las colas de baja prioridad y de alta prioridad no estén vacías */
    if(queue_empty(queue_low) == 0 || queue_empty(queue_high) == 0) {
      enable_interrupt();
      TCB* new_thread = scheduler();
      activator(new_thread);
    }
    enable_interrupt();
  }
  /* En caso de que el id no sea -1, comprobamos que el thread que se está ejecutando es de baja prioridad */
  else if(t_state[i].priority == LOW_PRIORITY) {
    /* Reducimos el número de ticks que le quedan al proceso que se está ejecutando para terminar */
    t_state[i].ticks--;
    /* Aplicamos las rodajas del RR a los procesos de baja prioridad */
    if(t_state[i].ticks == 0) {
      /* Restablecemos su valor de ticks a QUANTUM_TICKS */
      t_state[i].ticks = QUANTUM_TICKS;
      disable_interrupt();
      /* Encolamos el proceso en la cola de baja prioridad */
      enqueue(queue_low, &t_state[i]);
      enable_interrupt();
      TCB* new_thread = scheduler();
      activator(new_thread);
    }
    disable_interrupt();
    /* Consultamos que la cola de alta prioridad no esté vacía */
    if(queue_empty(queue_high) == 0) {
      enable_interrupt();
      /* Restablecemos su valor de ticks a QUANTUM_TICKS */
      t_state[i].ticks = QUANTUM_TICKS;
      disable_interrupt();
      enqueue(queue_low, &t_state[i]);
      enable_interrupt();
      TCB* new_thread = scheduler();
      /* Expulsamos un proceso de baja prioridad para ejecutar un proceso de alta prioridad */
      if(new_thread->priority == HIGH_PRIORITY) {
        printf("*** THREAD %d PREEMTED: SETCONTEXT OF %d\n", last_id, new_thread->tid);
      }
      activator(new_thread);
    }
    enable_interrupt();
  }
}

/* Activator */
void activator(TCB* next) {
  /* Comprobamos que el thread que está haciendo el activator no sea el thread last */
  if(last != next) {
    /* Realizamos un cambio de contexto desde el thread idle a un thread listo para ejecutar */
    if(last->tid == -1) {
      printf("*** THREAD READY: SET CONTEXT TO %d\n", next->tid);
      if(setcontext(&(next->run_env)) == -1) {
        perror("*** ERROR: error in command setcontext\n");
        exit(-1);
      }
    }
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
