#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>
#include <sys/select.h>

/* 
*	Error codes for get_response().
*/
#define SS_NO_MEMORY	0
#define SS_CLOSE_CONN	1
#define SS_WOULD_BLOCK	2

/** 
*	\brief	Calls send() in a loop to ensure that all data is sent. 
*	\param	slave_fd - The file descriptor to send to.
*	\param	len - To store he number of bytes actually sent.
*	\param	line - A pointer to a line to send.
*	\return 0 on success, or -1 on failure.
*/
int send_internal (int slave_fd, const char *line, size_t *len);
void send_response (size_t nbytes, const char *line, int sender_fd,
                           int master_fd, fd_set master, int fd_max);

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
char *get_response (size_t *nbytes, int slave_fd,
                           unsigned *err_code);

#endif /* NETWORK_H */
