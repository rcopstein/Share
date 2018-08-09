//
// # Entry Module
//
// Handles command line arguments and initializes processes
// responsible for managing the network
//

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "output.h"

int main(int argc, char** argv) {

    // Check for minimum number of arguments
    if (argc < 2) {
        printf("Print Usage Here\n");
        return 1;
    }

    // Declare arguments
    char ip[16] = {0};
    char name[51] = {0};
    uint16_t port = 4000;
    char description[257] = {0};

    // Read arguments
    for (uint8_t i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {

            if (strcmp(argv[i], "-ip") == 0) {
                if (++i < argc) strncpy(ip, argv[i], sizeof(char) * 15);
            }

            else if (strcmp(argv[i], "-p") == 0) {
                if (++i < argc) port = (uint16_t) atoi(argv[i]);
                else {
                    printf("Found '-p' flag but value is missing!\n");
                    return 1;
                }
            }

            else if (strcmp(argv[i], "-d") == 0) {
                if (++i < argc) strncpy(description, argv[i], sizeof(char) * 256);
                else {
                    printf("Found '-d' flag but value is missing!\n");
                    return 1;
                }
            }

            else warning("Flag '%s' not defined!", argv[i]);

        } else {
            if (name[0] == '\0') strncpy(name, argv[i], sizeof(char) * 50);
            else warning("Unknown parameter: '%s'", argv[i]);
        }
    }

    // Check if required values were provided

    if (name[0] == '\0') return error("\nRequired parameter 'name' was not provided!\n");
    if (ip[0] == '\0') return error("Required parameter 'ip' was not provided");

    // Print values
    printf("Name: %s\n", name);
    printf("Description: %s\n", description);
    printf("IP: %s\n", ip);
    printf("Port: %d\n", port);

}
