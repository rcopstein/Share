#include <string.h>
#include <errno.h>

#include "protocol_freq.h"
#include "hierarchy.h"
#include "output.h"
#include "system.h"
#include "nops.h"

static const char protocol[] = "freq";
static const uint8_t protocol_size = 4;

int send_freq_req(const char* type, member* m, char* param1, char* param2) {

    // Define Type

    uint16_t type_size   = (uint16_t) strlen(type);
    uint16_t param1_size = (uint16_t) strlen(param1);
    uint16_t param2_size = (uint16_t) strlen(param2);

    // Calculate Size

    size_t size = sizeof(uint16_t) * 2;
    size += protocol_size;
    size += param1_size;
    size += param2_size;
    size += type_size;

    // Allocate Message

    char* message = (char *) malloc(sizeof(char) * size);
    char* aux = message;

    // Write Message

    memcpy(aux, protocol, protocol_size); // Write Protocol
    aux += protocol_size;

    memcpy(aux, type, type_size); // Write Request Type
    aux += type_size;

    memcpy(aux, &param1_size, sizeof(uint16_t)); // Write Param1 Size
    aux += sizeof(uint16_t);

    memcpy(aux, param1, param1_size); // Write Param1
    aux += param1_size;

    memcpy(aux, &param2_size, sizeof(uint16_t)); // Write Param2 Size
    aux += sizeof(uint16_t);

    memcpy(aux, param2, param2_size); // Write Param2
    aux += param2_size;

    // Open Socket

    int socket = nops_open_connection(m->ip, m->port);

    // Send Message

    int16_t result = (int16_t) nops_send_message(socket, message, size);
    if (result != NOPS_SUCCESS) { printf("Result sending message %d\n", result); goto END; }

    result = (int16_t) nops_read_message(socket, (void **) &message, &size);
    if (result != NOPS_SUCCESS) { printf("Response from member %d\n", result); goto END; }

    memcpy(&result, message, sizeof(int16_t));

    END:

    nops_close_connection(socket);
    free(message);
    return result;
}
int send_freq_rep(int16_t response, int socket) {

    return nops_send_message(socket, &response, sizeof(int16_t));

}

void handle_freq_add(uint16_t path_size, char* path, uint16_t name_size, char* name, int socket) {

    member* current = get_current_member();

    LogicalFile* file = create_lf(false, name, current->id, name);
    char* npath = resolve_path(file);

    printf("Creating at %s\n", npath);

    // TODO: Attempt to recreate the file while result is EEXIST

    become_user();
    int res = open(npath, O_CREAT | O_EXCL, 0755);
    become_root();

    if (res == -1) { printf("Error opening file %d!\n", errno); res = -errno; }
    else {
        if ((res = add_lf(file, path))) { printf("Error adding file to lhier!\n"); remove(npath); res = -ENOENT; }
        else inc_lhier_seq_num();
    }

    free(npath);
    send_freq_rep((uint16_t) res, socket);

    printf("Response for FREQ ADD was %d\n", res);
}
void handle_freq_protocol(char* message, int socket) {

    char type[4];
    char *param1, *param2;
    uint16_t param1_size, param2_size;

    memcpy(type, message, sizeof(char) * 3); // Read Type
    message += sizeof(char) * 3;
    type[3] = '\0';

    memcpy(&param1_size, message, sizeof(uint16_t)); // Read Param1 Size
    message += sizeof(uint16_t);

    param1 = (char *) malloc(param1_size + 1); // Allocate Param1
    strncpy(param1, message, param1_size); // Read Param1
    message += param1_size;

    memcpy(&param2_size, message, sizeof(uint16_t)); // Read Param2 Size
    message += sizeof(uint16_t);

    param2 = (char *) malloc(param2_size + 1); // Allocate Param2
    strncpy(param2, message, param2_size); // Read Param2
    // message += param2_size;

    if (!strcmp(type, FREQ_ADD)) handle_freq_add(param1_size, param1, param2_size, param2, socket);
    // if (!strcmp(type, FREQ_DEL)) handle_freq_del(message);
    // if (!strcmp(type, FREQ_REN)) handle_freq_ren(message);

}
