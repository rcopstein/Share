//
// Standard Output Module
//

#include <stdio.h>

#include "output.h"

#define NRM  "\x1B[0m"
#define RED  "\x1B[31m"
#define GRN  "\x1B[32m"
#define YEL  "\x1B[33m"
#define BLU  "\x1B[34m"
#define MAG  "\x1B[35m"
#define CYN  "\x1B[36m"
#define WHT  "\x1B[37m"

int warning(char* comment, void* param) {
    printf(YEL);
    printf(comment, param);
    printf(NRM "\n");
    return 0;
}

int error(char* comment, void* param) {
    printf(RED);
    printf(comment, param);
    printf(NRM "\n");
    return 1;
}
