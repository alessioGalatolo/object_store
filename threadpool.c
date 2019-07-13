/*
	Progetto SOL AA 2018/19
	Alessio Galatolo
*/

#include <threadpool.h>
#include <stdlib.h>

#define THREAD_CHECK(x)\
	if((x) != 0)return 1;


/*
	initializes threadpool
	returns 0 on success, 1 on failure
*/
int tp_init(threadpool_t* tp){
	tp -> tlist = NULL;
	if(pthread_mutex_init(&(tp -> mutex), NULL) != 0)
		return 1;
	return 0;
}

/*
	spawns thread with the fun passaed as param and adds the id to the list
	returns 0 on success, 1 on failure
*/
int tp_spawn(threadpool_t* tp, void*(*fun) (void*), void* arg){
	pthread_t tid;
	pthread_attr_t attr;
	THREAD_CHECK(pthread_attr_init(&attr));
	THREAD_CHECK(pthread_create(&tid, &attr, fun, arg));
	THREAD_CHECK(pthread_mutex_lock(&(tp -> mutex)));

	if(tp -> tlist == NULL){
		if((tp -> tlist = malloc(sizeof(list_t))) == NULL)
			return 1;
		tp -> tlist -> tid = tid;
		tp -> tlist -> next = NULL;
		THREAD_CHECK(pthread_mutex_unlock(&(tp -> mutex)));
		return 0;
	}
	list_t* cur = tp -> tlist;
	while(cur -> next != NULL)
		cur = cur -> next;
	if((cur -> next = malloc(sizeof(list_t))) == NULL){
		THREAD_CHECK(pthread_mutex_unlock(&(tp -> mutex)));
		return 1;
	}
	cur -> next -> tid = tid;
	cur -> next -> next = NULL;
	THREAD_CHECK(pthread_mutex_unlock(&(tp -> mutex)));
	return 0;
}

/*
	joins with all the threads and frees the memory
	returns 0 on success, some int on failure
*/
int tp_terminate(threadpool_t* tp){
	THREAD_CHECK(pthread_mutex_lock(&(tp -> mutex)));
	list_t* cur = tp -> tlist;
	int outcome = 0;
	while(cur != NULL){
		outcome += pthread_join(cur -> tid, NULL);
		list_t* tofree = cur;
		cur = cur -> next;
		free(tofree);
	}
	THREAD_CHECK(pthread_mutex_unlock(&(tp -> mutex)));
	THREAD_CHECK(pthread_mutex_destroy(&(tp -> mutex)));
	return outcome;
}
