#include "pbx.h"
#include "server.h"
#include "debug.h"

#include "csapp.h"

void *pbx_client_service(void *arg) {

    // ignore SIGPIPE signal
    if(signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal error");
        exit(EXIT_FAILURE);
    }

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
    while((c = fgetc(in)) != EOF) {

        if(ferror(in) != 0) { // EOF read
            debug("end");
            break;
        }

        str[len] = c;
        len = len + 1;
        str = realloc(str, (len+1)*sizeof(char));

        if(c == '\n' && str[len-2] == '\r') {
            str[len-2] = '\0';
            len = len -1;
            if(strcmp(str, tu_command_names[TU_PICKUP_CMD]) == 0) {
                tu_pickup(client);
            } else if(strcmp(str, tu_command_names[TU_HANGUP_CMD]) == 0) {
                tu_hangup(client);
            } else {
                if(str[4] == ' ') { // space
                    char *word = malloc(5*sizeof(char));
                    strncpy(word, str, 4);
                    word[4] = '\0';
                    if(strcmp(word, tu_command_names[TU_DIAL_CMD]) == 0) {
                        word = realloc(word, (len-5)*sizeof(char));
                        strcpy(word, str+5);
                        tu_dial(client, atoi(word));
                    } else if(strcmp(word, tu_command_names[TU_CHAT_CMD]) == 0) {
                        word = realloc(word, (len-5)*sizeof(char));
                        strcpy(word, str+5);
                        tu_chat(client, word);
                    }
                    free(word);
                }
            }
            len = 0;
        }
    }

    Close(connfd); //close in
    free(str);
    return NULL;
}