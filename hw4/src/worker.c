#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "debug.h"
#include "polya.h"

void sigterm_handler(int);
void sighup_handler(int);

void sigterm_handler(int sig) {
    debug("Received signal 15 - Exited");
    // abandon any current solution attempt
    exit(EXIT_SUCCESS); //ternimate normally with status EXIT_SUCCESS = EXITED
}

volatile sig_atomic_t canceled = 0;
void sighup_handler(int sig) {
    canceled = 1;
    debug("Received signal 1 - Aborted");
    // stop attempt and send 'failed' "result" to master = ABORTED
}

/*
 * worker
 * (See polya.h for specification.)
 */
int worker(void) {
    // master redirects worker stdin and stdout to pipes
    // worker reads from stdin writes to stdout

    // initialization = STARTED
    debug("Starting");

    if(signal(SIGTERM, sigterm_handler) == SIG_ERR) {
        perror("signal error");
        exit(EXIT_FAILURE);
    }

    sigset_t mask_all, mask_hup, prev_mask;
    sigfillset(&mask_all);
    sigemptyset(&mask_hup);
    sigaddset(&mask_hup, SIGHUP);
    if(signal(SIGHUP, sighup_handler) == SIG_ERR) {
        perror("signal error");
        exit(EXIT_FAILURE);
    }

    if(raise(SIGSTOP) !=0) { // IDLE
        perror("raise error");
        exit(EXIT_FAILURE);
    }
    debug("Idling - sent SIGSTOP to itself");

    //SIGCONT = CONTINUED // master
    debug("Continuing");
    // loop - repearedly receive a problem from master process to solve
    while(1) {
        // RUNNING
        debug("Running");
        debug("Reading problem");
        // get problem
        struct problem* problem_ptr = malloc(sizeof(struct problem)); // allocated space for probem = header + data
        if(problem_ptr == NULL) {
            perror("malloc error");
            exit(EXIT_FAILURE);
        }
        fread(problem_ptr, sizeof(struct problem), 1, stdin);// read stdin to get header
        //ferror //eof includes short count, which is not an error
        if(ferror(stdin)) {
            perror("fread error");
            exit(EXIT_FAILURE);
        }
        problem_ptr = realloc(problem_ptr, problem_ptr -> size); // realloc size
        if(problem_ptr == NULL) {
            perror("realloc error");
            exit(EXIT_FAILURE);
        }
        fread(problem_ptr->data, (problem_ptr -> size - sizeof(struct problem)), 1, stdin);// continue to read stdin to get 'data' = total size - header size
        //ferror
        if(ferror(stdin)) {
            perror("fread error");
            exit(EXIT_FAILURE);
        }

        debug("Solving Problem");
        // attempts to solve problem
        // BLOCK SIGHUP - result may be unintialized
        sigprocmask(SIG_BLOCK, &mask_hup, &prev_mask);
        struct result* result_ptr = (void*)(solvers[problem_ptr -> type].solve(problem_ptr, &canceled));
        // UNBLOCK
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        // until 1) solution is found
        // 2) solution procedure fails
        // 3) master process notifies worker to cancel solution procedure - SIGHUP
            if(canceled == 1) {
                // stop attempt and send 'failed' "result" to master = ABORTED
                result_ptr->failed = 1;
                canceled = 0;
            }

        // send back "result" to the master process = write result to stdout
        fwrite(result_ptr, result_ptr->size, 1, stdout);
        //ferror
        if(ferror(stdout)) {
            //close
            //exit
            perror("fwrite error");
            exit(EXIT_FAILURE);
        }
        if(fflush(stdout) == EOF) {
            perror("fflush error");
            exit(EXIT_FAILURE);
        }
        //close? *********************************


        // free data that was allocated (when solution found)
        free(problem_ptr);

        // stop itself and wait for a new problem to be sent by the master process = STOPPED
        if(raise(SIGSTOP) !=0) {
            perror("raise error");
            exit(EXIT_FAILURE);
        }
        debug("Stopped");
    }

    return EXIT_FAILURE; //********************
}
