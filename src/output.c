//
// Standard Output Module
//

#include <stdio.h>

#include "output.h"

int warning(char* comment, void* param) {
    printf(YEL);
    printf(comment, param);
    printf(NRM);
    fflush(0);
    return 0;
}

int error(char* comment, void* param) {
    printf(RED);
    printf(comment, param);
    printf(NRM);
    fflush(0);
    return 1;
}
