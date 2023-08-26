#include "client_info.h"

#include <string.h>
#include <arpa/inet.h>

#define SENTINEL_VALUE -1

void init_clients (int size, struct client_info slaves[size],
                              struct client_info *p_slaves[size])
{
    for (int i = 0; i < size; i++) {
        slaves[i].id = slaves[i].sock = SENTINEL_VALUE;
        p_slaves[i] = &slaves[i];
    }
}


int find_empty_slot (struct client_info *const *slaves, int size)
{
    for (int i = 0; i < size; i++) {
        if (slaves[i]->id == SENTINEL_VALUE) {
            return i;
        }
    }
    /* UNREACHED. */
    return -1;
}

void fill_client_entry (int slave_fd, int id, int entry,
                        struct client_info *const *slaves,
                        const struct client_info *client_info)
{
    memcpy (slaves[entry]->address, client_info->address,
            INET6_ADDRSTRLEN);
    slaves[entry]->id = id;
    slaves[entry]->sock = slave_fd;
}

void clear_client_entry (int entry, struct client_info *const *slaves)
{
    memset (slaves[entry]->address, 0x00, INET6_ADDRSTRLEN);
    slaves[entry]->id = SENTINEL_VALUE;
    slaves[entry]->sock = SENTINEL_VALUE;
}

int comp_client_address (const void *s, const void *t)
{
    const struct client_info *const *p = s;
    const struct client_info *const *q = t;

    return memcmp ((*p)->address, (*q)->address, INET6_ADDRSTRLEN);
}

int comp_client_sock (const void *s, const void *t)
{
    const struct client_info *const *p = s;
    const struct client_info *const *q = t;

    return ((*p)->sock > (*q)->sock) - ((*p)->sock < (*q)->sock);
}



