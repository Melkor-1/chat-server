#ifndef CLIENT_INFO_H
#define CLIENT_INFO_H

#include <arpa/inet.h>

/*
*	A struct to keep track of an IP's state.
*/
struct client_info {
    char address[INET6_ADDRSTRLEN];
    int id;
    int sock;
};

void init_clients (int size, struct client_info slaves[size],
                              struct client_info *p_slaves[size]);
int find_empty_slot (struct client_info *const *slaves, int size);
void fill_client_entry (int slave_fd, int id, int entry,
                        struct client_info *const *slaves,
                        const struct client_info *client_info);
void clear_client_entry (int entry, struct client_info *const *slaves);
int comp_client_address (const void *s, const void *t);
int comp_client_sock (const void *s, const void *t);

#endif /* CLIENT_INFO__H */
