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

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <stdint.h>

#include <unistd.h>
#include <sys/time.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "log.h"

#if defined(__GNUC__)
#	define PRINTF_LIKE(n, m)       __attribute__((format(printf, n, m)))
#else
#	define PRINTF_LIKE(n, m)       /* If only */
#endif  /* __GNUC__ */

#define PROGRAM_NAME "selectserver"
#define PORT "9909"             /* Port we are listening on. */
#define MAX_LOG_TEXT 2048       /* Max text length for logging. */

#ifdef  BUFSIZ                  /* Max client response length. */
#define BUFSIZE BUFSIZ
#else
#define BUFSIZE 4096
#endif

#define LOG_FILE "server.log"
#define MAX_SLAVES 1022
#define ARRAY_CARDINALITY(x) (sizeof(x) / sizeof ((x)[0]))

#ifndef NI_MAXHOST
#define NI_MAXHOST 1025
#endif

#ifndef NI_MAXSERV
#define NI_MAXSERV 32
#endif

#define LOG_MSG(stream, msg, flags) log_msg (stream, msg, flags); \
											log_msg (0, msg, flags)

/* 
*	Error codes for get_response().
*/
#define SS_NO_MEMORY	0
#define SS_CLOSE_CONN	1
#define SS_WOULD_BLOCK	2

/*
*	Different types of logs.
*/
static const char *const logs[] = {
    "%s: We only sent %zu bytes because of a send() error.\n",
#define SS_SEND_ERROR		0
    "%s: INFO: Socket %d hung up.",
#define SS_CLOSED_CONN		1
    "%s: This server can not handle any more connections at this moment.\n",
#define	SS_CONN_SURPLUS		2
    "%s: Couldn't send the peer the full message.\n",
#define SS_FAILED_EXCUSE 	3
    "%s: INFO: New connection from HOST:%s, SERVICE:%s,\n\t\t\t\t\t   LOCAL IP ADDRESS:%s, on socket %d.",
#define SS_NEW_CONN			4
    "%s: Server overloaded. Caution advised.\n",
#define SS_OVERLOAD			5
    "%s: Failed to setup a socket.\n",
#define SS_SOCKET_ERROR		6
    "%s: fclose() failed. Logs might have been lost.\n",
#define SS_FCLOSE_ERROR		7
    "\n%s:Listening for connections on port %s.\n",
#define SS_INITIATE			8
};

/*
*	A struct to keep track of an IP's state.
*/
struct info {
    char address[INET6_ADDRSTRLEN];
    int id;
    int sock;
};

/*
*	File descriptor set for pipe(). 
*/
static int pfds[2] = { 0 };

/*
*	A FILE * for the log file. 
*/
static FILE *log_fp = 0;

static inline int max (int x, int y)
{
    return x > y ? x : y;
}

PRINTF_LIKE(3, 4) static void err_ret (FILE *stream, unsigned level, const char *fmt, ...)
{
    char buf[BUFSIZE];
    va_list argp;
    va_start (argp, fmt);

	if (fmt) {
    	vsnprintf (buf, sizeof buf, fmt, argp);
    	LOG_MSG (stream, buf, level);
	}
    va_end (argp);
}

static void sig_handler (int sig)
{
    const int saved_errno = errno;

    if (write (pfds[1], "x", 1) == -1
        && (errno != EAGAIN || errno != EWOULDBLOCK)) {
        signal (sig, SIG_DFL);
        raise (sig);
    }
    errno = saved_errno;
}

static void close_descriptor (int fd)
{
    if (close (fd) == -1) {
        perror ("close()");
    }
}

/**
*	\brief	Sets a socket to non-blocking mode.
*	\param	fd - A file descriptor
*	\return 0 if it succeeds, or -1 on failure.
*/
static int enable_nonblocking (int fd)
{
    int cmd = F_GETFL;
    int flags;

	/* The loop would exit after the second iteration because fcntl()
	*  would return 0.
	*/
    for (flags = 0; flags = fcntl (fd, cmd, flags); flags |= O_NONBLOCK) {
        cmd = F_SETFL;
    }
    return flags;
}

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

struct info *ss_search (int size, struct info *p_slaves[size],
                        struct info *const *ptr, int (*func) (const void *,
                                                              const void *))
{
    for (int i = 0; i < size; i++) {
        if (!func (ptr, &p_slaves[i])) {
            return p_slaves[i];
        }
    }
    return 0;
}

/** 
*	\brief	Calls send() in a loop to ensure that all data is sent. 
*	\param	slave_fd - The file descriptor to send to.
*	\param	len - To store he number of bytes actually sent.
*	\param	line - A pointer to a line to send.
*	\return 0 on success, or -1 on failure.
*/
static int send_internal (int slave_fd, const char *line, size_t *len)
{
    size_t total = 0;
    size_t bytes_left = *len;
    ssize_t ret_val;

    for (ret_val = 0; total < *len && ret_val != -1;
         total += (size_t) ret_val) {
		/*
		 * Disable SIGHUP, because a closed connection would generate a 
		 * send error, which would kill the server process.
		 */
        ret_val = send (slave_fd, line + total, bytes_left, MSG_NOSIGNAL);
        bytes_left -= (size_t) ret_val;
    }
    *len = total;
    return ret_val == -1 ? -1 : 0;
}

static void send_response (size_t nbytes, const char *line, int sender_fd,
                           int master_fd, fd_set master, int fd_max)
{
    for (int i = 0; i <= fd_max && FD_ISSET (i, &master); i++) {
        /*
         * Send it to everyone. 
         */
        if (i != master_fd && i != sender_fd && i != pfds[0]) {
            /*
             * Excluding the master, sender, and the write end of the pipe. 
             */
            size_t len = nbytes;

            if (send_internal (i, line, &len) == -1) {
                perror ("send()");
            } else if (len != nbytes) {
                err_ret (log_fp, LOG_FULLTIME, logs[SS_SEND_ERROR],
                         PROGRAM_NAME, len);
            }
        }
    }
}

/** 
*	\brief	Get the number of bytes that are immediately available for reading.
*	\param	slave_fd - Socket to peek.	
*   \return	The number of bytes available, or -1 on failure.
*/
static int get_bytes (int slave_fd)
{
    int flag = 0;

    if (ioctl (slave_fd, FIONREAD, &flag) == -1) {
        perror ("ioctl()");
        return -1;
    }
    return flag;
}

/**
*	\brief	 Calls recv() in a loop to read as much as available. 	
*  	\param	 nbytes	  - To store the number of bytes read.
*  	\param	 slave_fd - The file descriptor to receive from.
*	\param	 err_code - An out pointer to hold the error code in case of failure.

*  	In case of an error, all allocated memory is freed and err_code
*  	is set to one of the following:
* 
*  	1) SS_NO_MEMORY 	- The system is out of memory.	
*  	2) SS_CLOSE_CONN 	- There was a recv() or fcntl() error.
*  	3) SS_WOULD_BLOCK	- The socket has been marked unblocking, but a subsequent
*					  	  call to recv() would block.
*
*  	\return	 A pointer to the line, or NULL on failure.
*	
*   \warning The caller is responsible for freeing the returned memory in
*  			 case of success, else we risk exhaustion.
*/
static char *get_response (size_t *nbytes, int slave_fd,
                           unsigned *err_code)
{
    /*
     * This is an arbitrary limit.
     * Does anyone know how to do this without a limit? 
     */
    const size_t page_size = BUFSIZE;
    char *buf = 0;
    int flag = 0;
    ssize_t ret_val = 0;
    size_t total = 0;
	
    do {
        if (total > (BUFSIZE * 10)) {
            /*
             * Likely a DOS attack.
             */
            *err_code = SS_CLOSED_CONN;
            goto out_free;
        }
        char *new = realloc (buf, total + page_size);

        if (!new) {
            *err_code = SS_NO_MEMORY;
            perror ("realloc()");
            goto out_free;
        }
        buf = new;

		/*
		* Disable SIGHUP, because a dropped connection causes a write error, which 
		* would make server process exit.
		*/
        if ((ret_val = recv (slave_fd, buf + total, page_size - 1, MSG_NOSIGNAL)) > 0) {
            total += (size_t) ret_val;
            buf[ret_val] = '\0';

            if ((flag = get_bytes (slave_fd)) == -1) {
                *err_code = SS_CLOSED_CONN;
                goto out_free;
            }
        } else {
            flag = !flag;
        }
    } while (flag > 0);

    if (ret_val == 0) {
        err_ret (log_fp, LOG_FULLTIME, logs[SS_CLOSED_CONN],
                 PROGRAM_NAME, slave_fd);
        *err_code = SS_CLOSED_CONN;
        goto out_free;
    }
    if (ret_val == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            *err_code = SS_WOULD_BLOCK;
            goto out_free;
        }
        *err_code = SS_CLOSED_CONN;
        goto out_free;
    }
    *nbytes = total;
    return buf;

  out_free:
    free (buf);
    return 0;
}

static void excuse_server (int slave_fd)
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

static int generate_entry (struct info *const *slaves, int size)
{
    for (int i = 0; i < size; i++) {
        if (slaves[i]->id == -1) {
            return i;
        }
    }
    /* UNREACHED. */
    return -1;
}

static void fill_entry (int slave_fd, int id, int entry,
                        struct info *const *slaves,
                        const struct info *client_info)
{
    memcpy (slaves[entry]->address, client_info->address,
            INET6_ADDRSTRLEN);
    slaves[entry]->id = id;
    slaves[entry]->sock = slave_fd;
}

static void clear_entry (int entry, struct info *const *slaves)
{
    memset (slaves[entry]->address, 0x00, INET6_ADDRSTRLEN);
    slaves[entry]->id = -1;
    slaves[entry]->sock = -1;
}

static int comp_address (const void *s, const void *t)
{
    const struct info *const *p = s;
    const struct info *const *q = t;

    return memcmp ((*p)->address, (*q)->address, INET6_ADDRSTRLEN);
}

static int comp_sock (const void *s, const void *t)
{
    const struct info *const *p = s;
    const struct info *const *q = t;

    return ((*p)->sock > (*q)->sock) - ((*p)->sock < (*q)->sock);
}

static void write_slave_info (int slave_fd, struct info *client_info)
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

static void init_struct_info (int size, struct info slaves[size],
                              struct info *p_slaves[size])
{
    /*
     * We will use -1 as the sentinel value.
     */
    for (int i = 0; i < size; i++) {
        slaves[i].id = slaves[i].sock = -1;
        p_slaves[i] = &slaves[i];
    }
}

/**
*	\brief 	 Accepts a new connection.
*	\param	 master_fd - The listening server socket.
*	\param   client_info - To store the slave IP address.
*	\return	 The slave file descriptor on success, or -1 on failure.
*/
static int accept_new_connection (int master_fd, struct info *client_info)
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

static int read_pipe (void)
{
    char ch = '\0';

    if (read (pfds[0], &ch, 1) == -1
        && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return 0;
    }
    return -1;
}

static void remove_existing_connection (fd_set * master, int max,
                                        int slave_fd,
                                        struct info *slave_info,
                                        struct info *p_slaves[],
                                        int *n_slaves)
{
    const struct info *key;

    for (key = 0;
         key =
         ss_search (*n_slaves, p_slaves, &slave_info, comp_address);) {
        FD_CLR (key->sock, master);
        close_descriptor (key->sock);
        clear_entry (key->id, p_slaves);
        (*n_slaves)--;
    }
    fill_entry (slave_fd, generate_entry (p_slaves, max), *n_slaves,
                p_slaves, slave_info);
}

static int close_log_file (void)
{
    if (fclose (log_fp) == EOF) {
        err_ret (log_fp, LOG_FULLTIME, logs[SS_FCLOSE_ERROR],
                 PROGRAM_NAME);
    }
    return -1;
}

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

    struct info slaves[MAX_SLAVES] = { 0 };
    struct info *p_slaves[MAX_SLAVES] = { 0 };

    init_struct_info (MAX_SLAVES, slaves, p_slaves);

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
                    struct info slave_info = { 0 };

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
                            struct info slave_info = {.sock = i };
                            struct info *p_slave_info = &slave_info;
                            struct info *key =
                                ss_search (n_slaves, p_slaves,
                                           &p_slave_info, comp_sock);

                            if (!key) {
                                break;
                            }
                            clear_entry (key->id, p_slaves);
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
static int setup_server (void)
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

/**
*	\brief	Opens the LOG_FILE, and sets the stream associated with it to line-buffered mode.
*	\return	0 on success, or -1 on failure.
*/
static int open_logfile (void)
{
    if (!(log_fp = fopen (LOG_FILE, "a"))) {
        perror ("fopen()");
        return -1;
    }
    return 0;
}

/**
*	\brief	Enables line buffering for the stream associated with the LOG_FILE.
*	\return 0 on success, or -1 on failure.
*/
static int enable_line_buf (void)
{
    /*
     * Be less efficient here, to solve a problem more important
     * than mere efficiency.
     */
	errno = 0;

    if (setvbuf (log_fp, 0, _IOLBF, 0)) {
        if (errno) {
			perror ("setvbuf()");
		}
		return -1;
    }
    return 0;
}

static int set_pipe_nonblock (void)
{
    int status = 0;

    for (size_t i = 0; i < ARRAY_CARDINALITY (pfds); i++) {
        if ((status = enable_nonblocking (pfds[i])) == -1) {
            break;
        }
    }
    return status;
}

static void close_pipe (void)
{
    for (size_t i = 0; i < ARRAY_CARDINALITY (pfds); i++) {
        close_descriptor (pfds[i]);
    }
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
