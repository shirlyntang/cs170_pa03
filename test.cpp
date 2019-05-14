#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>
using namespace std;

#define PASS 1
#define FAIL 0

static int a41;
static int a42;
static void* _thread_slow_join(void* arg){
    sleep(1);
    a42 = 12;
    pthread_exit(0);
}
static void* _thread_fast_join(void* arg){
    a41 = 11;
    pthread_exit(0);
}

static int test3(void){
    pthread_t tid1; pthread_t tid2;
    void *rtn1, *rtn2;

    pthread_create(&tid1, NULL,  &_thread_fast_join, NULL);
    pthread_create(&tid2, NULL,  &_thread_slow_join, NULL);

    cerr << "d" << endl;

    pthread_join(tid2, &rtn2);

    cerr << "g" << endl;
    pthread_join(tid1, &rtn1);

    cerr << "f" << endl;

    if (a41 != 11 || a42 != 12)
        return FAIL;
    return PASS;
}

int main()
{
    cerr << "ESRCH: " << ESRCH << endl;
    cerr << "EINVAL: " << EINVAL << endl;
    cerr << "EDEADLK: " << EDEADLK << endl;
    int result = test3();
    cerr << "result: " << result << endl;
    return 0;
}
