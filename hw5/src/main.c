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

static void terminate(int status);

static void server_shutdown_handler(int signum) {
    terminate(EXIT_FAILURE);
}

/*
 * "PBX" telephone exchange simulation.
 *
 * Usage: pbx <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
    int option;
    int port;
    while((option = getopt(argc, argv, "p:")) != EOF) {
        switch(option) {
            case 'p':
                if((port = atoi(optarg++)) < 1024) {
                    fprintf(stderr, "-p (port) requires a valid port number\n");
                    exit(EXIT_FAILURE);
                }
                break;
        }
    }
    if(option == -1) { // fail
        fprintf(stderr, "Usage: bin/pbx -p <port>\n");
        exit(EXIT_FAILURE);
    }

    // Perform required initialization of the PBX module.
    debug("Initializing PBX...");
    pbx = pbx_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function pbx_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.

    /*fprintf(stderr, "You have to finish implementing main() "
	    "before the PBX server will function.\n");

    terminate(EXIT_FAILURE);*/

    struct sigaction sa;
    sa.sa_handler = server_shutdown_handler;
    sigaction(SIGHUP, &sa, NULL);

    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    listenfd = Open_listenfd(argv[2]);
    while(1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Pthread_create(&tid, NULL, pbx_client_service, connfdp);
    }

}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    debug("Shutting down PBX...");
    pbx_shutdown(pbx);
    debug("PBX server terminating");
    exit(status);
}
