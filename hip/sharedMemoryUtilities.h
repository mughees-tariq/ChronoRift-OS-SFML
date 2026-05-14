//////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "sharedState.h"

//////////////////////////////////////////////////////////////////////////////////////////

//creating shm and initialize all semaphores and zero state (Arbiter only):
int initializeSharedMemory(SharedState** out);

//attaching to existing shm (HIP + ASP):
int attachStateToSharedMemory(SharedState** out);

//detaching from shm (all processes on exit):
void detachFromSharedMemory(SharedState* s);

//destroying shm segment and semaphores (Arbiter only, call last):
void destroySharedMemory(int shmId, SharedState* s);

//writing entries to action log with proper synchronization:
void logAction(SharedState* s, const char* msg, pid_t pid, pthread_t tid);

//////////////////////////////////////////////////////////////////////////////////////////
