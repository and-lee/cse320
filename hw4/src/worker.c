#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "debug.h"
#include "polya.h"

void sigterm_handler(int);
void sighup_handler(int);

void sigterm_handler(int sig) {
    // abandon any current solution attempt
    exit(EXIT_SUCCESS); //ternimate normally with status EXIT_SUCCESS = EXITED
}

volatile sig_atomic_t canceled = 0;
void sighup_handler(int sig) {
    canceled = 1;
    //write result, aborted.
    exit(EXIT_FAILURE); // stop attempt and send 'failed' result to master. exit? = ABORTED
}

/*
 * worker
 * (See polya.h for specification.)
 */
int worker(void) {
    // master redirects worker stdin and stdout to pipes
    // worker reads from stdin writes to stdout

    if(signal(SIGTERM, sigterm_handler) == SIG_ERR)
        perror("signal error");
    if(signal(SIGHUP, sighup_handler) == SIG_ERR)
        perror("signal error");

    // initialization = STARTED
    //
    raise(SIGSTOP); // IDLE

    //SIGCONT = CONTINUED // master
    // loop - repearedly receive a problem from master process to solve
    //
    // RUNNING
    // get problem
    struct problem* problem_ptr = malloc(sizeof(struct problem)); // allocated space for probem = header + data
    ////if(problem_ptr == NULL)
    fread(problem_ptr, sizeof(struct problem), 1, stdin);// read stdin to get header
    ////ferror //eof includes short count, which is not an error
    problem_ptr = realloc(problem_ptr, problem_ptr -> size); // realloc size
    ////if(problem_ptr == NULL)
    fread(problem_ptr->data, (problem_ptr -> size - sizeof(struct problem)), 1, stdin);// continue to read stdin to get 'data' = total size - header size
    ////ferror

    // attempts to solve problem
    struct result* result_ptr = (void*)(solvers[problem_ptr -> type].solve(problem_ptr, &canceled));
    // until 1) solution is found
    // 2) solution procedure fails
    // 3) master process notifies worker to cancel solution procedure - SIGHUP *************
    //debug("DDDDDDDDDD %d", result_ptr->failed);
    //if (result_ptr->failed == 0) {
    //}
    //debug("SSSSSSSSSSSSS %ld", result_ptr->size);
    //debug("IIIIIIIIIIIIIIII %d", result_ptr->id);
    //result_ptr->data
    fwrite(result_ptr, result_ptr->size, 1, stdout);
    fflush(stdout);


// free DATA that was allocated ****************************** when solution found

    // send back "result" to the master process
    // write result to stdout


    // catch SIGHUP signal while trying to solve problem = stop attempt and send 'failed' result to master
        //if current solution attempt has not already succeeded/failed - then it's abandoned and send failed to master
            //EXIT_FAILURE (done before STOP signal) = ABORTED

    raise(SIGSTOP); // stop itself and wait for a new problem to be sent by the master process = STOPPED


    return EXIT_FAILURE;
}
