#ifndef SERVER_H
#define SERVER_H

#include "client_info.h"

#include <sys/select.h>
#include <netdb.h>

/**
*	\brief 	 Accepts a new connection.
*	\param	 master_fd - The listening server socket.
*	\param   client_info - To store the slave IP address.
*	\return	 The slave file descriptor on success, or -1 on failure.
*/
int accept_new_connection (int master_fd, struct client_info *client_info);

// int open_tcp_socket (struct addrinfo *const *servinfo);

/**
*	\brief	Opens a new file descriptor, binds to it, and set it to listening mode.
*	\return	A new socket descriptor on success, or -1 on failure. 
*/
int setup_server (void);

void excuse_server (int slave_fd);
void remove_existing_connection (fd_set * master, int max,
                                        int slave_fd,
                                        struct client_info *slave_info,
                                        struct client_info *p_slaves[],
                                        int *n_slaves);

#endif /* SERVER_H */
