#pragma once

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#endif

// Define Semaphore Struct
typedef struct _semaphore {

#ifdef __APPLE__
    dispatch_semaphore_t    sem;
#else
    sem_t                   sem;
#endif

} semaphore;

// Define Methods

void portable_sem_init(semaphore *s, uint32_t value);

void portable_sem_wait(semaphore *s);

void portable_sem_post(semaphore *s);

void portable_sem_destroy(semaphore *s);
