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

    FILE *in;
    if((in = fdopen(connfd, "r")) == NULL) { // write problem
        perror("unable to create output stream");
        exit(EXIT_FAILURE);
    }

    // enter service loop, repeatedly reveive a message sent by the client
    // parse the message, carry out specified command -PBX module
    // service loop ends when network connection shuts down and EOF is seen
    int c;
    char *str = malloc(sizeof(char));
    int len = 0;
    int dial = 0;
    int chat = 0;
    while((c = fgetc(in)) != EOF) {

        if(ferror(in) != 0) { // EOF read
            debug("end");
            break;
        }

        str[len] = c;
        len = len + 1;
        str = realloc(str, (len+1)*sizeof(char));

        /*if(strcmp(str, tu_command_names[TU_DIAL_CMD]) == 0) {
            debug("dialED");
            // int tu_dial(TU *tu, int ext): Dial an extension on a TU.
            // error *
            // space
            if((c=fgetc(in)) == ' ') {
                dial = 1;
                len = 0;
            }
        }
        if(strcmp(str, tu_command_names[TU_CHAT_CMD]) == 0) {
            debug("chatED");
            // int tu_chat(TU *tu, char *msg): "Chat" over a connection.
            // space ?
            chat = 1;
            len = 0;
        }*/
        //if(c == ' ') {
            //debug("first space");
            char *word = malloc((len+1)*sizeof(char));
            strcpy(word, str);
            word[len-1] = '\0';
            if(strcmp(word, tu_command_names[TU_DIAL_CMD]) == 0) {
                //debug("dialED");
                // int tu_dial(TU *tu, int ext): Dial an extension on a TU.
                // error *
                // space
                dial = 1;
                len = 0;
            }
            if(strcmp(word, tu_command_names[TU_CHAT_CMD]) == 0) {
                //debug("chatED");
                // int tu_chat(TU *tu, char *msg): "Chat" over a connection.
                // space ?
                chat = 1;
                len = 0;
            }
            free(word);
        //}

        if(c == '\n' && str[len-2] == '\r') {
            str[len-2] = '\0';
            //debug("x %s", str);

            if(strcmp(str, tu_command_names[TU_PICKUP_CMD]) == 0) {
                tu_pickup(client);
            } else if(strcmp(str, tu_command_names[TU_HANGUP_CMD]) == 0) {
                tu_hangup(client);
            } else {
                if(dial) {
                    //debug("d %s", str);
                    tu_dial(client, atoi(str));
                    dial = 0;
                } else if(chat) {
                    //debug("c %s", str);
                    tu_chat(client, str);
                    chat = 0;
                }
            }

            len = 0;
        }
    }

    Close(connfd); //close in
    free(str);
    return NULL;
}