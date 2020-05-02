#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "pbx.h"
#include "server.h"
#include "debug.h"

#include "csapp.h"

/*
 * Thread function for the thread that handles a particular client.
 *
 * @param  Pointer to a variable that holds the file descriptor for
 * the client connection.  This variable must be freed once the file
 * descriptor has been retrieved.
 * @return  NULL
 *
 * This function executes a "service loop" that receives messages from
 * the client and dispatches to appropriate functions to carry out
 * the client's requests.  The service loop ends when the network connection
 * shuts down and EOF is seen.  This could occur either as a result of the
 * client explicitly closing the connection, a timeout in the network causing
 * the connection to be closed, or the main thread of the server shutting
 * down the connection as part of graceful termination.
 */
void *pbx_client_service(void *arg) {
    int connfd = *((int *)arg); // get fd
    Pthread_detach(pthread_self()); // detach thread so it does not have to be explicitly reaped
    Free(arg); // free storage

    // register client fd with PBX module
    TU* client = pbx_register(pbx, connfd);

    // enter service loop
        // repeatedly reveive a message sent by the client
            // parse the message
                // carry out specified command -(PBX module)

    // service loop ends when network connection shuts down and EOF is seen
    // EOF is not a character
    // check return value from fgetc/fgets
        // check feof(), ferror()


    /*char c;
    //while(1) outside until EOF
    FILE *in;
    if((in = fdopen(connfd, "r")) == NULL) { // read result
        perror("unable to create input stream");
        exit(EXIT_FAILURE);
    }
    // read and write write ON HOOK, RINGING, DIAL TONE
    while((c = fgetc(in)) != EOF) {
    }*/
size_t n;
char buf[MAXLINE];
rio_t rio;

Rio_readinitb(&rio, connfd);
while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
    // buffer includes the terminating new line character
        // and terminates the text line with null (zero) character
    buf[strcspn(buf, EOL)] = 0;
    //Rio_writen(connfd, buf, n);
    if(strcmp(buf, tu_command_names[TU_PICKUP_CMD]) == 0) {
        tu_pickup(client);
    } else if(strcmp(buf, tu_command_names[TU_HANGUP_CMD]) == 0) {
        tu_hangup(client);
    } else if(strcmp(buf, tu_command_names[TU_DIAL_CMD]) == 0) {
        tu_dial(client, 0);
    } else if(strcmp(buf, tu_command_names[TU_CHAT_CMD]) == 0) {
        tu_chat(client, buf);
    }

}


    // ^]
    // unresgister
    //pbx_unregister(pbx, client);

    /*
    //PBX *pbx_init():  Initialize a new PBX.
    //pbx_shutdown(PBX *pbx):  Shut down a PBX.
    TU *pbx_register(PBX *pbx, int fd):  Register a TU client with a PBX.
    int pbx_unregister(PBX *pbx, TU *tu):  Unregister a TU from a PBX.
    int tu_fileno(TU *tu): Get the file descriptor for the network connection underlying a TU.
    int tu_extension(TU *tu): Get the extension number for a TU.

    int tu_pickup(TU *tu): Take a TU receiver off-hook (i.e. pick up the handset).
    int tu_hangup(TU *tu): Hang up a TU (i.e. replace the handset on the switchhook).
    int tu_dial(TU *tu, int ext): Dial an extension on a TU.
    int tu_chat(TU *tu, char *msg): "Chat" over a connection.
    */

    Close(connfd);
    return NULL;
}