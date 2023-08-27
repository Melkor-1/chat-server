#include "internal.h"

/*
*   An array of log message templates for different types of events.	
*/
const char *const logs[] = {
    [SS_SEND_ERROR] =
        "%s: [ ERROR ]: We only sent %zu bytes because of a send() error.\n",
    [SS_CLOSED_CONN] = 
        "%s: [ INFO ]: Socket %d hung up.",
    [SS_CONN_SURPLUS] = 
        "%s: [ WARNING ]: This server can not handle any more connections at this moment.\n",
    [SS_FAILED_EXCUSE] = 
        "%s: [ ERROR ]: Couldn't send the peer the full message.\n",
    [SS_NEW_CONN] =
        "%s: [ INFO ]: New connection from HOST:%s, SERVICE:%s,\n\t\t\t\t\t   LOCAL IP ADDRESS:%s, on socket %d.",
    [SS_OVERLOAD] =     
        "%s: [ WARNING ]: Server overloaded. Caution advised.\n",
    [SS_SOCKET_ERROR] =  
        "%s: [ ERROR ]: Failed to setup a socket.\n",
    [SS_FCLOSE_ERROR] =     
        "%s: [ ERROR ]: fclose() failed. Logs might have been lost.\n",
    [SS_INITIATE] =     
        "%s: [ INFO ]: Listening for connections on port %s.\n",
};


