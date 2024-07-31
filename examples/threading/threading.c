#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
	int res;
	// TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
	// hint: use a cast like the one below to obtain thread arguments from your parameter
	struct thread_data* thread_func_args = (struct thread_data *) thread_param;
	
	// WAIT - in ms
	usleep(thread_func_args->wait_to_obtain_ms*1000);
	
	// OBTAIN MUTEX
	res = pthread_mutex_lock(thread_func_args->mutex);
	if (res != 0) { 
		ERROR_LOG("Locking mutex has failed with %d", res);
	}
	
	// WAIT - in ms
	usleep(thread_func_args->wait_to_release_ms*1000);
	
	// RELEASE MUTEX
	res = pthread_mutex_unlock(thread_func_args->mutex);
	if (res != 0) { 
		ERROR_LOG("Unlocking mutex has failed with %d", res);
	}
	
	thread_func_args->thread_complete_success = true;
		
	return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
	int res;
	/**
	* TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
	* using threadfunc() as entry point.
	*
	* return true if successful.
	*
	* See implementation details in threading.h file comment block
	*/
	
	// ALLOCATE MEMORY
	struct thread_data* thread_func_args = (struct thread_data *) malloc(sizeof(struct thread_data));
	if (thread_func_args == NULL){
		ERROR_LOG("Memory Allocation failed for thread");
		return false;
	}
	
	// SETUP MUTEX AND WAITS
	thread_func_args->mutex = mutex;
	thread_func_args->wait_to_obtain_ms = wait_to_obtain_ms;
	thread_func_args->wait_to_release_ms = wait_to_release_ms;
	
	// PASS THREAD TO CREATED THREAD
	res = pthread_create(thread, // thread
				NULL, // attr
				threadfunc, // routine
				(void*)thread_func_args // args
				);
	if (res != 0) { 
		ERROR_LOG("Unlocking mutex has failed with %d", res);
		return false;
	}
	
	return true;
}

