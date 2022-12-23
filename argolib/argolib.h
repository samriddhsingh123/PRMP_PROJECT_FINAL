#ifndef __ARGOLIB_H__
#define __ARGOLIB_H__

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdarg.h>
#include <abt.h>
#include <pthread.h>
#include <inttypes.h>


typedef ABT_thread Task_handle;
typedef void (*fork_t)(void* args);
void argolib_init(int argc, char **argv);

void argolib_finalize();

void argolib_kernel(fork_t fptr, void* args);

Task_handle* argolib_fork(fork_t fptr, void* args);

void argolib_join(Task_handle** list, int size);
void argolib_start_tracing();
void argolib_stop_tracing();

#endif