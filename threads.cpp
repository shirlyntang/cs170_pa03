#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <vector>
#include <unistd.h>
#include <iostream>
#include <semaphore.h>
#include <errno.h>
using namespace std;

#define READY 0
#define RUNNING 1
#define BLOCKED 2
#define EXITED 3

#define SEM_VALUE_MAX 65536

struct tcb
{
    void *SP;
    void *(*func)(void*);
	  pthread_t tid; //thread id
    jmp_buf buf; // jmp_buf of thread
    int status = 0; //status of thread
    void* args;
    void* return_value;
    tcb* waiting_thread = NULL;
};

struct semaphore
{
  int val;
  int status;
  bool initialized = false;
  //tcb_list list;
  vector<tcb*> list;
};

static vector<tcb> threads;
static vector<tcb> exited_threads;
int vector_index = 0;
pthread_t thread_id = 0;
int current_thread = 0;
sigset_t alarm_set;
long int* pointer = 0;
bool locked = false;

void lock() {
  sigemptyset(&alarm_set);
  sigaddset(&alarm_set, SIGALRM);
	sigprocmask(SIG_BLOCK, &alarm_set, NULL);
  locked = true;
}

void unlock() {
  if (!locked)
    return;
  sigemptyset(&alarm_set);
  sigaddset(&alarm_set, SIGALRM);
  locked = false;
	sigprocmask(SIG_UNBLOCK, &alarm_set, NULL);
}

static long int i64_ptr_mangle(long int p);
void scheduler();
void* wrapper_function();
void switch_threads(int sig);
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg);
void pthread_exit(void *value_ptr);
pthread_t pthread_self(void);

sem_t sem;

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
  lock();
  if (sem == NULL)
  {
  	unlock();
  	return -1;
  }
  if (pshared != 0)
  {
    unlock();
    return ENOSYS;
  }
  semaphore* s = new semaphore();
  if (value > SEM_VALUE_MAX)
  {
    unlock();
    cerr << "here" << endl;
    return EINVAL;
  }
  s->val = value;
  s->status = READY;
  s->initialized = true;
  s->list.resize(0, 0);
  sem->__align = (long int)s;
  unlock();
  return 0;
}
int sem_destroy(sem_t *sem)
{
  lock();
  if (sem == NULL || ((semaphore*)(sem->__align))->initialized == false)
  {
  	unlock();
  	return -1;
  }
  if (sem->__align!=0 && ((semaphore*)(sem->__align))->status != BLOCKED)
  {
    delete((semaphore*)(sem->__align));
    sem->__align = 0;
  }
  unlock();
  return 0;
}
int sem_wait(sem_t *sem)
{
  lock();
  if (sem == NULL)
  {
  	unlock();
  	return -1;
  }
  if (((semaphore*)(sem->__align))->val > 0)
  {
    (((semaphore*)(sem->__align))->val)--;
  }
  else
  {
    cerr << "current thread: " << current_thread << endl;
    cerr << "threads.size(): " << threads.size() << endl;
    ((semaphore*)(sem->__align))->list.push_back(&threads[current_thread]);
    threads[current_thread].status = BLOCKED;
    ((semaphore*)(sem->__align))->status = BLOCKED;
    switch_threads(SIGALRM);
  }
  unlock();
  return 0;
}
int sem_post(sem_t *sem)
{
  lock();
  if (sem == NULL)
  {
  	unlock();
  	return -1;
  }
  if (((semaphore*)(sem->__align))->val >= SEM_VALUE_MAX)
  {
    unlock();
    return EOVERFLOW;
  }
  else if (((semaphore*)(sem->__align))->list.size() > 0)
  {
    ((semaphore*)(sem->__align))->list[0]->status = READY;
    ((semaphore*)(sem->__align))->list.erase(((semaphore*)(sem->__align))->list.begin());
  }
  else
  {
    ((semaphore*)(sem->__align))->val++;
    ((semaphore*)(sem->__align))->status = READY;
  }
  unlock();
  return 0;
}


static long int i64_ptr_mangle(long int p)
{
    long int ret;
    asm(" mov %1, %%rax;\n"
        " xor %%fs:0x30, %%rax;"
        " rol $0x11, %%rax;"
        " mov %%rax, %0;"
    : "=r"(ret)
    : "r"(p)
    : "%rax"
    );
    return ret;
}

void scheduler()
{
  //cerr << "scheduler current: " << current_thread << endl;
  //threads[current_thread].status = READY;
	if(current_thread >= threads.size()-1)
  	{
  		current_thread = 0;
  	}
  	else
  	{
  		current_thread++;
  	}
    while(threads[current_thread].status == BLOCKED || threads[current_thread].status == EXITED)
    {
      if (current_thread < threads.size()-1)
        current_thread++;
      else
        current_thread = 0;
    }
    threads[current_thread].status = RUNNING;
  	longjmp(threads[current_thread].buf, 1); //TODO: change status of thread???
}

void* wrapper_function()
{

	void* (*start_routine)(void*) = threads[current_thread].func;
	pthread_exit(start_routine(threads[current_thread].args));
}

void switch_threads(int sig)
{
  //cerr << "ALARM----------------------------------------" << endl;
	int i;
	i = setjmp(threads[current_thread].buf);

	if (i != 0)
  		return;

  	scheduler();
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg)
{
	tcb main_tcb;
  if (threads.size() != 0){
		main_tcb = threads[0];
	}
	if(!setjmp(main_tcb.buf))
	{
		if (thread_id == 0)
		{
			main_tcb.tid = thread_id;
			main_tcb.status = RUNNING; //change to the RUNNABLE thing
			thread_id++;
      threads.push_back(main_tcb);

			struct sigaction sigact;
	    sigaddset(&alarm_set, SIGALRM);
			sigact.sa_flags = SA_NODEFER;
			sigact.sa_handler = switch_threads;
			sigaction(SIGALRM, &sigact, NULL);

      sigemptyset( &sigact.sa_mask );
      sigemptyset(&alarm_set);
      sigaddset(&alarm_set, SIGALRM);

			ualarm(5000, 5000);
		}
    cerr << "CREATE" << endl;
		tcb new_thread;
		void* reserved_memory = malloc(32767);
		long int* thread_memory = (long int*)reserved_memory;
    if (thread_memory > pointer)
      pointer = thread_memory;
    new_thread.SP = reserved_memory;
    new_thread.args = arg;
    new_thread.tid = thread_id;
    new_thread.func = start_routine;

		thread_memory+= 32765/8-2; // go to top of stack
    setjmp(new_thread.buf);
		*(((unsigned long int*)(&(new_thread.buf)))+6) = i64_ptr_mangle((long int)thread_memory);
		*(((unsigned long int*)(&(new_thread.buf)))+7) = i64_ptr_mangle((long int) &wrapper_function);
    threads.push_back(new_thread);

		*thread=thread_id;
		thread_id++;

	}
  else
  {
    cerr << "after longjmp to main" << endl;
    pthread_exit(0);
  }
	return 0;
}

int pthread_join(pthread_t thread, void **value_ptr)
{
  if (threads.size() == 0)
    return EDEADLK;
  cerr << "join called" << endl;
  int target = -1;

  //ERROR CHECKING
  if (thread == threads[current_thread].tid)
  {
    return EDEADLK;
  }
  lock();
  for (int i = 0; i < threads.size(); i++)
  {
    if (thread == threads[i].tid)
    {
      target = i;
      break;
    }
  }
  if (target == -1)
  {
    return ESRCH;
  }
  tcb* target_thread = &threads[target];
  unlock();

  if (target_thread->status == EXITED)
  {
    value_ptr = &(target_thread->return_value);
    return ESRCH;
  }

  lock();
  if (target_thread->waiting_thread != NULL)
    return EINVAL;
  threads[current_thread].status = BLOCKED;
  target_thread->waiting_thread = &threads[current_thread];
  if (setjmp(threads[current_thread].buf)!= 0)
  {
      if (value_ptr != NULL)
  		  *value_ptr = target_thread->return_value;
  }
  else
  {
    unlock();
    scheduler();
  }

  unlock();
  return 0;
}

void pthread_exit(void *value_ptr)
{
	if (threads.size()-1 < 0)
  {
		exit(1);
  }
	else if (threads.size()-1 == 0)
	{
    if (threads[0].SP != pointer)
		  free(threads[0].SP);
    threads[0].return_value = value_ptr;
    threads[0].status = EXITED;
    threads[0].waiting_thread->status = READY;
		exit(0);
	}
	else
	{
    if (threads[current_thread].SP != pointer)
    {
      free(threads[current_thread].SP);
    }
    threads[current_thread].return_value = value_ptr;
    threads[current_thread].status = EXITED;
    if (threads[current_thread].waiting_thread != NULL)
      threads[current_thread].waiting_thread->status = READY;
	}
  current_thread++;

  if (current_thread == threads.size() && threads.size() > 0)
  {
    current_thread = 0;
  }
  while(threads[current_thread].status == BLOCKED || threads[current_thread].status == EXITED)
  {
    if (current_thread < threads.size()-1)
      current_thread++;
    else
      current_thread = 0;
  }
	longjmp(threads[current_thread].buf, 1);
	exit(0);
}

pthread_t pthread_self(void)
{
	return threads[current_thread].tid;
}
