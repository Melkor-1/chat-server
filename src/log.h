#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define LOG_TIME            0x01        /* 0b00000001 */
#define LOG_DATE            0x02        /* 0b00000010 */
#define LOG_USER            0x04        /* 0b00000100 */
#define LOG_COUNT           0x08        /* 0b00001000 */
#define LOG_ALL             0xFF        /* 0b11111111 */
#define LOG_FULLTIME        0x03        /* 0b00000011 */

/** 
*	@brief	log_msg() writes msg to stream in the form:
*			"03-02-16 (Thu), 10:05:41, msg", if msg is not NULL. If stream is NULL,
*			the writes to stderr instead.
*  	@param	stream - A FILE * to write to.
*	@param	msg    - A pointer to a line.
*  	@param	flags  - The argument flags must include one (or more) of the following:
*					 LOG_COUNT, LOG_DATE, LOG_TIME, LOG_FULLTIME, or LOG_ALL.
*  	@return Upon successful return, log_msg() returns the number of characters printed.
*			If an output error is encountered, a negative value is returned. 
*/
extern int log_msg (FILE *stream, const char *msg, unsigned options);

#endif
