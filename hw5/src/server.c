#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "pbx.h"
#include "server.h"
#include "debug.h"

#include "csapp.h"

void *pbx_client_service(void *arg) {
    int connfd = *((int *)arg); // get fd
    Pthread_detach(pthread_self()); // detach thread so it does not have to be explicitly reaped
    Free(arg); // free storage

    // register client fd with PBX module
    TU* client = pbx_register(pbx, connfd);

    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    // enter service loop, repeatedly reveive a message sent by the client
    // parse the message, carry out specified command -PBX module
    Rio_readinitb(&rio, connfd);
    // service loop ends when network connection shuts down and EOF is seen
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        // buffer includes the terminating new line character
            // and terminates the text line with null (zero) character
        buf[strcspn(buf, EOL)] = 0;
        if(strcmp(buf, tu_command_names[TU_PICKUP_CMD]) == 0) {
            tu_pickup(client);
        } else if(strcmp(buf, tu_command_names[TU_HANGUP_CMD]) == 0) {
            tu_hangup(client);
        } else {
            char *ptr = strchr(buf, ' ');
            char word[MAXLINE];
            char msg[MAXLINE]; // msg -> buf
            strncpy(word, buf, ptr-buf);
            strcpy(msg, ptr+1);
            //debug("word %s\n", word);
            //debug("buf %s\n", buf);
            //debug("msg %s\n", msg);
            if(strcmp(word, tu_command_names[TU_DIAL_CMD]) == 0) {
                // int tu_dial(TU *tu, int ext): Dial an extension on a TU.
                tu_dial(client, atoi(msg));
                // error
            } else if(strcmp(word, tu_command_names[TU_CHAT_CMD]) == 0) {
                // int tu_chat(TU *tu, char *msg): "Chat" over a connection.
                tu_chat(client, msg);
            } else {
                exit(EXIT_FAILURE);
            }
        }
    }

    Close(connfd);
    return NULL;
}