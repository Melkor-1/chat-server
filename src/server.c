#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif

#define _POSIX_C_SOURCE 200819L
#define _XOPEN_SOURCE   700



#include "server.h"
#include "client_info.h"
#include "err.h"
#include "internal.h"
#include "network.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/tcp.h>


static void configure_tcp (int slave_fd)
{
    struct option {
        int level;
        int opt_name;
        const void *const opt_val;
    } const options[] = {
        { SOL_SOCKET, SO_KEEPALIVE, (int[]) {1} },
        { IPPROTO_TCP, TCP_KEEPCNT, (int[]) {9} },
        { IPPROTO_TCP, TCP_KEEPIDLE, (int[]) {25} },
        { IPPROTO_TCP, TCP_KEEPINTVL, (int[]) {25} },
    };

    for (size_t i = 0; i < ARRAY_CARDINALITY (options); i++) {
        if (setsockopt
            (slave_fd, options[i].level, options[i].opt_name,
             options[i].opt_val, sizeof (int)) == -1) {
            perror ("setsockopt()");
        }
    }
}

void excuse_server (int slave_fd)
{
    const size_t len = strlen (logs[SS_CONN_SURPLUS]);
    size_t nbytes = len;

    if (send_internal (slave_fd, logs[SS_CONN_SURPLUS], &nbytes) == -1) {
        perror ("send()");
    } else if (nbytes != len) {
        err_ret (log_fp, LOG_FULLTIME, logs[SS_FAILED_EXCUSE],
                 PROGRAM_NAME);
    }
}

static void write_slave_info (int slave_fd, struct client_info *client_info)
{
    struct sockaddr slave_addr = { 0 };
    socklen_t addr_len = sizeof slave_addr;
    char local_ip[INET6_ADDRSTRLEN] = { 0 };

    if (getsockname (slave_fd, &slave_addr, &addr_len) == -1) {
        perror ("getsockname()");
        return;
    }
    if (!inet_ntop (AF_INET6, &slave_addr, local_ip, sizeof local_ip)) {
        perror ("inet_ntop()");
        return;
    }
    char host[NI_MAXHOST] = { 0 };
    char service[NI_MAXSERV] = { 0 };
    int ret_val = 0;

    if ((ret_val =
         getnameinfo (&slave_addr, addr_len, host,
                      sizeof host, service, sizeof service,
                      NI_NUMERICHOST | NI_NUMERICSERV)) != 0) {
        err_ret (log_fp, LOG_FULLTIME, "%s: getnameinfo(): %s\n",
                 PROGRAM_NAME, gai_strerror (ret_val));
        return;
    }
    err_ret (log_fp, LOG_FULLTIME, logs[SS_NEW_CONN], PROGRAM_NAME, host,
             service, local_ip, slave_fd);
    memcpy (client_info->address, local_ip, sizeof local_ip);
}
/**
*	\brief 	 Accepts a new connection.
*	\param	 master_fd - The listening server socket.
*	\param   client_info - To store the slave IP address.
*	\return	 The slave file descriptor on success, or -1 on failure.
*/
int accept_new_connection (int master_fd, struct client_info *client_info)
{
    int slave_fd = 0;
    struct sockaddr slave_addr = { 0 };
    socklen_t addr_len = sizeof slave_addr;

    if ((slave_fd = accept (master_fd, &slave_addr, &addr_len)) == -1) {
        perror ("accept()");
        goto fail;
    }
    configure_tcp (slave_fd);

    if (enable_nonblocking (slave_fd) == -1) {
        perror ("fcntl()");
        goto close_n_fail;
    }
    write_slave_info (slave_fd, client_info);
    return slave_fd;

  close_n_fail:
    close_descriptor (slave_fd);
  fail:
    return -1;
}

void remove_existing_connection (fd_set * master, int max,
                                        int slave_fd,
                                        struct client_info *slave_info,
                                        struct client_info *p_slaves[],
                                        int *n_slaves)
{
    const struct client_info *key;

    for (key = 0; key = ss_search (*n_slaves, p_slaves, &slave_info, comp_client_address); ) {
        FD_CLR (key->sock, master);
        close_descriptor (key->sock);
        clear_client_entry (key->id, p_slaves);
        (*n_slaves)--;
    }
    fill_client_entry (slave_fd, find_empty_slot (p_slaves, max), *n_slaves,
                p_slaves, slave_info);
}


/**
*	\brief 	Opens a TCP socket, binds to it, and sets it to listening and non-blocking mode.
*	\param	servinfo - A struct of type struct addrinfo.
*	\return A listening socket descriptor, or -1 on failure. 
*/
static int open_tcp_socket (struct addrinfo *const *servinfo)
{
    int master_fd = 0;
    struct addrinfo *p = 0;

    for (p = *servinfo; p; p = p->ai_next) {
        if ((master_fd =
             socket (p->ai_family, p->ai_socktype,
                     p->ai_protocol)) == -1) {
            perror ("socket()");
            continue;
        }
        if (setsockopt (master_fd, SOL_SOCKET, SO_REUSEADDR, (int[]) { 1 },
                        sizeof (int)) == -1) {
            perror ("setsockopt()");
        }
        if (bind (master_fd, p->ai_addr, p->ai_addrlen) == -1) {
            perror ("bind()");
            close_descriptor (master_fd);
            continue;
        }
        break;
    }

    if (!p) {
        err_ret (log_fp, LOG_FULLTIME, logs[SS_SOCKET_ERROR],
                 PROGRAM_NAME);
        goto fail;
    }
    if (enable_nonblocking (master_fd) == -1) {
        perror ("fcntl()");
        goto close_n_fail;
    }
    if (listen (master_fd, SOMAXCONN) == -1) {
        perror ("listen()");
        goto close_n_fail;
    }

    return master_fd;

  close_n_fail:
    close_descriptor (master_fd);
  fail:
    return -1;
}

static int init_addr (struct addrinfo **servinfo)
{
    const struct addrinfo hints = {.ai_family = AF_UNSPEC,.ai_socktype =
            SOCK_STREAM,
        .ai_flags = AI_PASSIVE
    };
    int ret_val = 0;

    if ((ret_val = getaddrinfo (0, PORT, &hints, servinfo)) != 0) {
        err_ret (log_fp, LOG_FULLTIME, "%s: getaddrinfo: %s.\n",
                 PROGRAM_NAME, gai_strerror (ret_val));
    }
    return ret_val ? -1 : 0;
}

/**
*	\brief	Opens a new file descriptor, binds to it, and set it to listening mode.
*	\return	A new socket descriptor on success, or -1 on failure. 
*/
int setup_server (void)
{
    struct addrinfo *servinfo;

    if (init_addr (&servinfo)) {
        goto fail;
    }

    const int master_fd = open_tcp_socket (&servinfo);

    if (master_fd == -1) {
        freeaddrinfo (servinfo);
        goto fail;
    }
    freeaddrinfo (servinfo);
    return master_fd;

  fail:
    return -1;
}

