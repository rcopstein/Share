#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <semaphore.h>

#include "shared_memory.h"

static sem_t* exports_file_semaphore = NULL;
static char* exports_file_semaphore_name = "etc_exports_mutex";

int lock_exports_file() {

    if (exports_file_semaphore == NULL) {
        exports_file_semaphore = sem_open(exports_file_semaphore_name, O_CREAT, O_RDWR, 1);
        if (exports_file_semaphore == NULL) return 1;
    }

    sem_wait(exports_file_semaphore);
    return 0;

}
int unlock_exports_file() {

    if (exports_file_semaphore == NULL) return 0;
    sem_post(exports_file_semaphore);
    return 0;

}
