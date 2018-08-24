#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "protocol_join.h"
#include "nfs_ops.h"
#include "members.h"
#include "output.h"

static void clean_protocol_join_received(int depth, char* id) {
    switch (depth) {
        case 1:
            free(id);
            break;
        default:
            break;
    }
}

void protocol_join_received(char* message) {

    // Temporary
    members = build_member("AA", "127.000.000.111", 4000, "/Users/rcopstein/Desktop");
    member result;

    char* buffer = NULL;
    print_member(members, &buffer);

    members->port = 0;
    parse_member(buffer, &result);

    printf("%d %s %s %d %d %s\n", result.id_size, result.id, result.ip, result.port, result.prefix_size, result.prefix);
    ++members_count;
    return;

    // Parse member in the message
    member m;
    if (parse_member(message, &m)) {
        error("Invalid member format in join protocol!\n", NULL);
        clean_protocol_join_received(0, NULL);
        return;
    }

    // Add it to NFS permissions
    if (add_nfs_recp(members->prefix, m.ip)) {
        error("Failed to add ip as NFS recipient!\n", NULL);
        clean_protocol_join_received(0, NULL);
        return;
    }

    // Assign an ID for it
    char* id = generate_member_id();

    // TO-DO Find a way to send numbers in a nice way

}
