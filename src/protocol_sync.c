#include <stdlib.h>

#include "protocol_sync.h"
#include "output.h"

static const char protocol[] = "sync";
static const uint8_t protocol_size = 4;

static int send_memb_sync_req(char* ip, uint16_t port, )
int send_sync_req(char* ip, uint16_t port, uint8_t type) {

    if (type == SYNC_MEMB) { /* Sync Members */ }
    return error("Invalid type for sync!\n", NULL);

}