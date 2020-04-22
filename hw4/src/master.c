#include <stdlib.h>
#include <signal.h>

#include "debug.h"
#include "polya.h"

/*void SIGSTOPhandler(int);
void SIGSTOPhandler(int sig) {
    //debug
    //exit(0);
}*/


/*
 * master
 * (See polya.h for specification.)
 */
int master(int workers) {
    // TO BE IMPLEMENTED
    sf_start(); // master process has begun to execute

    //signal(SIGSTOP, SIGSTOPhandler);
    sf_end(); // master process is about to terminate
    return EXIT_FAILURE;
}
