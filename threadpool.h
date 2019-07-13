/*
	Progetto SOL AA 2018/2019
	Alessio Galatolo
*/

/*
	A simple library handling thread spawns
*/


#if !defined(_THREAD_POOL__)

#define _THREAD_POOL__
#include <pthread.h>

typedef struct _list{
	pthread_t tid;
	struct _list* next;
}list_t;


typedef struct{
	pthread_mutex_t mutex;
	list_t* tlist;
}threadpool_t;

int tp_init(threadpool_t*);
int tp_spawn(threadpool_t*, void*(*)(void*), void*);
int tp_terminate(threadpool_t*);


#endif
