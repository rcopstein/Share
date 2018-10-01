#include <stdlib.h>
#include "semaphores.h"

void portable_sem_init(semaphore *s, uint32_t value)
{
#ifdef __APPLE__
    dispatch_semaphore_t *sem = &s->sem;
    *sem = dispatch_semaphore_create(value);
#else
    sem_init(&s->sem, 0, value);
#endif
}

void portable_sem_wait(semaphore *s)
{
#ifdef __APPLE__
    dispatch_semaphore_wait(s->sem, DISPATCH_TIME_FOREVER);
#else
    sem_wait(&s->sem);
#endif
}

void portable_sem_post(semaphore *s)
{
#ifdef __APPLE__
    dispatch_semaphore_signal(s->sem);
#else
    sem_post(&s->sem);
#endif
}

void portable_sem_destroy(semaphore *s)
{
#ifndef __APPLE__
    sem_destroy(&s->sem);
#endif
    free(s);
}
