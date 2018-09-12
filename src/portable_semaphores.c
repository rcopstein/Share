#include <stdlib.h>
#include "portable_semaphores.h"

void portable_sem_init(portable_semaphore *s, uint32_t value)
{
#ifdef __APPLE__
    dispatch_semaphore_t *sem = &s->sem;
    *sem = dispatch_semaphore_create(value);
#else
    sem_init(&s->sem, 0, value);
#endif
}

void portable_sem_wait(portable_semaphore *s)
{
#ifdef __APPLE__
    dispatch_semaphore_wait(s->sem, DISPATCH_TIME_FOREVER);
#else
    int r;
    do { r = sem_wait(&s->sem); }
    while (r == -1 && errno == EINTR);
#endif
}

void portable_sem_post(portable_semaphore *s)
{
#ifdef __APPLE__
    dispatch_semaphore_signal(s->sem);
#else
    sem_post(&s->sem);
#endif
}

void portable_sem_destroy(portable_semaphore *s)
{
#ifndef __APPLE__
    sem_destroy(s->sem);
#endif
    free(s);
}
