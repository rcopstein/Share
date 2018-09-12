#pragma once

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#endif

// Define Semaphore Struct
typedef struct _portable_semaphore {

#ifdef __APPLE__
    dispatch_semaphore_t    sem;
#else
    sem_t                   sem;
#endif

} portable_semaphore;

// Define Methods

void portable_sem_init(portable_semaphore *s, uint32_t value);

void portable_sem_wait(portable_semaphore *s);

void portable_sem_post(portable_semaphore *s);

void portable_sem_destroy(portable_semaphore *s);
