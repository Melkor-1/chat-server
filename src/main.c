/**
*	\file	selectserver.c
*
*	\author	Haris Salam 
*
*	\date	Wednesday, 2nd January 2023
*
*	\brief 	A multi-person chat server. 
*
*	\bug	No known bugs. 
*/

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif

#define _POSIX_C_SOURCE 200819L
#define _XOPEN_SOURCE   700

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>

#include "err.h"
#include "internal.h"
#include "network.h"
#include "pipe.h"
#include "utils.h"
#include "server.h"

/*
*	File descriptor set for pipe(). 
*/
int pfds[2] = { 0 };

/*
*	A FILE * for the log file. 
*/
FILE *log_fp = 0;




/**
*	\brief	Calls select() and handles new connections.
*	\param	master_fd - A listening socket.
*	\return 0 on SIGINT, or -1 on allocation or select() failure.
*/
static int handle_connections (int master_fd)
{
    fd_set master;              /* Master file descriptor list. */
    fd_set read_fds;            /* Temporary file descriptor list for select(). */

    FD_ZERO (&master);
    FD_ZERO (&read_fds);
    FD_SET (master_fd, &master);
    FD_SET (pfds[0], &master);

    int fd_max = master_fd;     /* Max file descriptor seen so far. */
    int n_slaves = 0;

    struct client_info slaves[MAX_SLAVES] = { 0 };
    struct client_info *p_slaves[MAX_SLAVES] = { 0 };

    init_clients (MAX_SLAVES, slaves, p_slaves);

    for (;;) {
        read_fds = master;
        if (select (fd_max + 1, &read_fds, 0, 0, 0) == -1) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            perror ("select()");
            return close_log_file ();
        }

        /*
         * Iterate through the existing connections looking for data to read.
         */
        for (int i = 0; i <= fd_max; i++) {
            /*
             * We have a connection. 
             */
            if (FD_ISSET (i, &read_fds)) {
                if (i == pfds[0] && (read_pipe () == -1)) {
                    /*
                     * Handler was called.
                     */
                    close_log_file ();
                    return 0;
                }
                if (i == master_fd) {
                    /*
                     * It's the master. 
                     */
                    struct client_info slave_info = { 0 };

                    int slave_fd =
                        accept_new_connection (master_fd, &slave_info);

                    if (slave_fd != -1 && slave_fd < FD_SETSIZE) {
                        /*
                         * We will forcibly close any existing connections from the new
                         * connection's IP address. This mean that any given attacking 
                         * computer could only tie up a maximum of one socket on the 
                         * server at a time, which would make it harder for that attacker
                         * to DOS the machine, unless the attacker has access to a number 
                         * of client machines.
                         */
                        remove_existing_connection (&master, MAX_SLAVES,
                                                    slave_fd, &slave_info,
                                                    p_slaves, &n_slaves);
                        n_slaves++;
                        FD_SET (slave_fd, &master);
                        fd_max = max (slave_fd, fd_max);
                    } else {
                        err_ret (log_fp, LOG_FULLTIME, logs[SS_OVERLOAD],
                                 PROGRAM_NAME);
                        excuse_server (slave_fd);
                        close_descriptor (slave_fd);
                    }
                } else {
                    /*
                     * We have data to read. 
                     */
                    size_t nbytes = 0;
                    unsigned err_code = 0;
                    char *const line =
                        get_response (&nbytes, i, &err_code);

                    if (!line) {
                        /*
                         * A read error, memory failure, or closed connection. 
                         * There is no good way to handle SS_WOULD_BLOCK.
                         */
                        if (err_code == SS_NO_MEMORY) {
                            return close_log_file ();
                        }
                        if (err_code == SS_CLOSE_CONN) {
                            struct client_info slave_info = {.sock = i };
                            struct client_info *p_slave_info = &slave_info;
                            struct client_info *key =
                                ss_search (n_slaves, p_slaves,
                                           &p_slave_info, comp_client_sock);

                            if (!key) {
                                break;
                            }
                            clear_client_entry (key->id, p_slaves);
                            n_slaves--;
                        }
                        FD_CLR (i, &master);
                        close_descriptor (i);
                    } else {
                        send_response (nbytes, line, i, master_fd, master,
                                       fd_max);
                        free (line);
                    }
                }
            }
        }
    }
    /* UNREACHED */
    return 0;
}

int main (void)
{
    static sigset_t caught_signals;

    static int const sig[] = {
        SIGALRM, SIGHUP, SIGINT,
        SIGPIPE, SIGQUIT, SIGTERM,
    };

    const size_t nsigs = ARRAY_CARDINALITY (sig);

    if (sigemptyset (&caught_signals) == -1) {
        perror ("sigemptyset()");
        goto fail;
    }

    struct sigaction act;

    for (size_t i = 0; i < nsigs; i++) {
        if (sigaction (sig[i], 0, &act) == -1) {
            perror ("sigaction()");
            goto fail;
        }
        if (act.sa_handler != SIG_IGN) {
            if (sigaddset (&caught_signals, sig[i]) == -1) {
                perror ("sigaddset()");
                goto fail;
            }
        }
    }
    act.sa_handler = sig_handler;
    act.sa_mask = caught_signals;
    act.sa_flags = SA_RESTART;

    for (size_t i = 0; i < nsigs; i++) {
        if (sigismember (&caught_signals, sig[i])) {
            if (sigaction (sig[i], &act, 0) == -1) {
                perror ("sigaction()");
                goto fail;
            }
        }
    }

    /*
     * Employ the self pipe trick so that we can avoid race conditions 
     * while both selecting on a set of file descriptors and also 
     * waiting for a signal.
     */
    if (pipe (pfds) == -1) {
        perror ("pipe()");
        goto fail;
    }
    if (set_pipe_nonblock () == -1) {
        perror ("fcntl()");
        goto close_n_fail;
    }
    if (open_logfile () == -1) {
        perror ("fopen()");
        goto close_n_fail;
    }
    if (enable_line_buf () == -1) {
        perror ("setvbuf()");
        goto close_all_n_fail;
    }
    const int master_fd = setup_server ();

    if (master_fd == -1) {
        goto close_all_n_fail;
    }
    /*
     * Wait for and eventually handle a new connection.
     */
    fprintf (stdout, logs[SS_INITIATE], PROGRAM_NAME, PORT);

    handle_connections (master_fd);
    close_descriptor (master_fd);

    return EXIT_SUCCESS;

  close_n_fail:
    close_pipe ();
    return EXIT_FAILURE;

  close_all_n_fail:
    close_pipe ();
    close_log_file ();

  fail:
    return EXIT_FAILURE;
}
