#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <serialization.h>

#include "protocol_freq.h"
#include "hierarchy.h"
#include "output.h"
#include "system.h"
#include "nops.h"

static const char protocol[] = "freq";
static const uint8_t protocol_size = 4;

int send_freq_req(const char *type, member *m, char *param1, char *param2, int flags) {

    // Define Type

    uint16_t type_size   = (uint16_t) strlen(type);
    uint16_t param1_size = (uint16_t) strlen(param1);
    uint16_t param2_size = (uint16_t) strlen(param2);

    // Calculate Size

    size_t size = sizeof(uint16_t) * 2;
    size += sizeof(uint32_t);
    size += protocol_size;
    size += param1_size;
    size += param2_size;
    size += type_size;

    // Test the size being passed
    if (size > UINT32_MAX) {
        printf("!!! SIZE WILL OVERFLOW !!!");
    }

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

    memcpy(aux, &flags, sizeof(uint32_t)); // Write flags
    // aux += sizeof(uint32_t);

    // Open Socket

    int socket = nops_open_connection(m->ip, m->port);

    // Send Message

    int16_t result = (int16_t) nops_send_message(socket, message, (uint32_t) size);
    if (result != NOPS_SUCCESS) { printf("Result sending message %d\n", result); goto END; }

    result = (int16_t) nops_read_message(socket, &message, (uint32_t *) &size);
    if (result != NOPS_SUCCESS) { printf("Result receiving message %d\n", result); goto END; }

    memcpy(&result, message, sizeof(int16_t));

    END:

    nops_close_connection(socket);
    free(message);
    return result;
}
int send_freq_req_add(member *m, char *path, char *name, int flags) {

    // Define Type

    uint16_t type_size   = (uint16_t) strlen(FREQ_ADD);
    uint16_t param1_size = (uint16_t) strlen(path);
    uint16_t param2_size = (uint16_t) strlen(name);

    // Calculate Size

    size_t size = sizeof(uint16_t) * 2;
    size += sizeof(uint32_t);
    size += protocol_size;
    size += param1_size;
    size += param2_size;
    size += type_size;

    // Test the size being passed
    if (size > UINT32_MAX) { printf("!!! SIZE WILL OVERFLOW !!!"); }

    // Allocate Message

    char* message = (char *) malloc(sizeof(char) * size);
    char* aux = message;

    // Write Message

    memcpy(aux, protocol, protocol_size); // Write Protocol
    aux += protocol_size;

    memcpy(aux, FREQ_ADD, type_size); // Write Request Type
    aux += type_size;

    memcpy(aux, &param1_size, sizeof(uint16_t)); // Write Param1 Size
    aux += sizeof(uint16_t);

    memcpy(aux, path, param1_size); // Write Param1
    aux += param1_size;

    memcpy(aux, &param2_size, sizeof(uint16_t)); // Write Param2 Size
    aux += sizeof(uint16_t);

    memcpy(aux, name, param2_size); // Write Param2
    aux += param2_size;

    memcpy(aux, &flags, sizeof(uint32_t)); // Write flags
    // aux += sizeof(uint32_t);

    // Open Socket

    int socket = nops_open_connection(m->ip, m->port);

    // Send Message

    int16_t result = (int16_t) nops_send_message(socket, message, (uint32_t) size);
    if (result != NOPS_SUCCESS) { printf("Result sending message %d\n", result); goto END; }

    result = (int16_t) nops_read_message(socket, &message, (uint32_t *) &size);
    if (result != NOPS_SUCCESS) { printf("Result receiving message %d\n", result); goto END; }

    if (message[0] == '\0') {

        result = -message[1];
        printf("FREQ Request ADD received error %d\n", result);

    } else {

        char* response;
        deserialize_string(message, &response);

        LogicalFile* file = create_lf(false, name, m->id, response);
        char* resolved_path = resolve_path(file);

        result = (int16_t) open(resolved_path, O_RDWR);
        free(resolved_path);
        free(response);

    }

    END:

    nops_close_connection(socket);
    free(message);
    return result;

}
int send_freq_req_ren(member* m, const char* from, const char* to) {
    return send_freq_req(FREQ_REN, m, (char *) from, (char *) to, 0);
}
int send_freq_req_del(member* m, const char* path) {

    char automatic[] = "y";
    if (m == get_current_member()) automatic[0] = 'n';
    return send_freq_req(FREQ_DEL, m, (char *) path, automatic, 0);
}

int send_freq_rep(int16_t response, int socket) {

    return nops_send_message(socket, &response, sizeof(int16_t));

}
int send_freq_rep_add(char* path, size_t size, int error, int socket) {

    if (error > 0) {

        char err[2];
        err[0] = '\0';
        err[1] = (char) error;
        return nops_send_message(socket, err, 2);

    }

    uint16_t serialized_size = sizeof_string(path);
    char* send = (char *) malloc(serialized_size);
    serialize_string(send, path, (uint16_t) strlen(path));

    int res = nops_send_message(socket, send, serialized_size);
    free(send);
    return res;

}

void handle_freq_add(char *path, char *name, uint32_t flags, int socket) {

    char* fullpath = (char *) malloc(strlen(path) + strlen(name) + 2);
    sprintf(fullpath, "%s/%s", path, name);

    LogicalFile* check = NULL;
    int err = _lf_get(fullpath, &check);
    free(fullpath);

    if (err == 0 && check != NULL) err = EEXIST;
    if (err != ENOENT) { send_freq_rep_add(NULL, 0, err, socket); return; }

    int res;
    LogicalFile* file = NULL;
    char *npath = NULL, *nname = NULL;
    member* current = get_current_member();

    do {

        if (file != NULL) free(file);
        if (nname != NULL) free(nname);

        struct timespec start;
        clock_gettime(CLOCK_REALTIME, &start);

        nname = (char *) malloc(sizeof(char) * 32);
        sprintf(nname, "%lu", start.tv_nsec);

        file = create_lf(false, name, current->id, nname);
        npath = resolve_path(file);

        // printf("Creating %s/%s at %s with size %zu\n", path, name, npath, strlen(npath));

        become_user();
        res = open(npath, flags, 0755);
        become_root();

    }
    while (res == -1 && errno == EEXIST);

    if (res == -1) send_freq_rep_add(NULL, 0, errno, socket);
    else {
        close(res);
        if (_lf_add(file, path, true)) { remove(npath); send_freq_rep_add(NULL, 0, ENOENT, socket); }
        else { inc_lhier_seq_num(); send_freq_rep_add(nname, strlen(nname), 0, socket); }
    }

    free(npath);
    free(nname);
}
void handle_freq_ren(char* from, char* to, int socket) {

    int res = 0;

    if (!(res = _lf_ren(from, to))) inc_lhier_seq_num();
    else res *= -1;

    send_freq_rep((int16_t) res, socket);

    // printf("Response for FREQ REN was %d\n", res);
}
void handle_freq_del(char* path, const char* ask_automatic, int socket) {

    printf("Got asked to delete %s\n", path);

    int res = 0;
    bool automatic = *ask_automatic != 'n';

    LogicalFile* file;
    int error = _lf_get(path, &file);
    if (file == NULL) { res = -error; goto END; }

    char* filepath = resolve_path(file);
    res = remove(filepath);
    free(filepath);

    if (res < 0) { res = -errno; goto END; }
    else if (!(res = _lf_rem(path, automatic))) inc_lhier_seq_num();
    else printf("RES was %d\n", res);

    END:

    send_freq_rep((int16_t) res, socket);

    printf("Response for FREQ DEL was %d\n", res);
}

void handle_freq_protocol(char* message, int socket) {

    char type[4];
    uint32_t flags;
    char *param1, *param2;
    uint16_t param1_size, param2_size;

    memcpy(type, message, sizeof(char) * 3); // Read Type
    message += sizeof(char) * 3;
    type[3] = '\0';

    memcpy(&param1_size, message, sizeof(uint16_t)); // Read Param1 Size
    message += sizeof(uint16_t);

    param1 = (char *) malloc(param1_size + 1); // Allocate Param1
    memcpy(param1, message, param1_size); // Read Param1
    param1[param1_size] = '\0';
    message += param1_size;

    memcpy(&param2_size, message, sizeof(uint16_t)); // Read Param2 Size
    message += sizeof(uint16_t);

    param2 = (char *) malloc(param2_size + 1); // Allocate Param2
    memcpy(param2, message, param2_size); // Read Param2
    param2[param2_size] = '\0';
    message += param2_size;

    memcpy(&flags, message, sizeof(uint32_t)); // Read flags
    // message += sizeof(uint32_t);

    if (!strcmp(type, FREQ_ADD)) handle_freq_add(param1, param2, flags, socket);
    if (!strcmp(type, FREQ_REN)) handle_freq_ren(param1, param2, socket);
    if (!strcmp(type, FREQ_DEL)) handle_freq_del(param1, param2, socket);

    free(param1);
    free(param2);
}
