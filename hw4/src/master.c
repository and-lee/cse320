#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>

#include "debug.h"
#include "polya.h"

volatile sig_atomic_t done;
volatile sig_atomic_t state[MAX_WORKERS];
volatile sig_atomic_t w_id[MAX_WORKERS];

int get_w_index(int pid) {
    for(int j = 0; j < MAX_WORKERS; j++) {
        if(w_id[j] == pid) {
            return j;
        }
    }
    return -1; // not found
}

// SIGCHLD handler notifies when worker processes stop and continue
void child_handler(int sig) {
    debug("SIGCHLD");
    int olderrno = errno;
    pid_t pid;
    if ((pid = waitpid(-1, NULL, WSTOPPED | WCONTINUED | WNOHANG)) != 0) {
        if(pid < 0) {
            perror("wait error");
        }
        int index = get_w_index(pid);
        debug("id = %d, STATE = %d", pid, state[index]);

        int prev_state = state[index];
        // change state
        if(prev_state  == WORKER_STARTED) {
            state[index] =  WORKER_IDLE; // set state = STARTED
        }
        if(prev_state  == WORKER_IDLE) {
            state[index] =  WORKER_CONTINUED; // set state = STARTED
        }
        if(prev_state  == WORKER_CONTINUED) {
            state[index] =  WORKER_RUNNING; // set state = STARTED
        }
        if(prev_state  == WORKER_RUNNING) {
            state[index] =  WORKER_STOPPED; // set state = STARTED
        }


        sf_change_state(pid, prev_state , state[index]);
    }
    errno = olderrno;
}

/*
 * master
 * (See polya.h for specification.)
 */
int master(int workers) {
    sf_start(); // master process has begun to execute
    if(workers < 1) {
        exit(EXIT_FAILURE);
    }

    // ignore SIGPIPE signal
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, child_handler); // install SIGCHLD handler

    // create new file objects
    int r_fd[workers];
    int w_fd[workers];

    int m_to_w_fd[2];
    int w_to_m_fd[2];
    //char buff [100];

    FILE /* *in,*/ *out;

    pid_t pid[workers];
    int i;

    for(i = 0; i < workers; i++) {
        // create 2 pipes for each worker
        // use pipe(2) before worker process is forked using fork(2)
            //send problems from master to worker
            //send results from worker to master
        if((pipe(m_to_w_fd) < 0) || (pipe(w_to_m_fd) < 0)) {
            perror("unable to create pipe");
            exit(1);
        }
        w_fd[i] = m_to_w_fd[1];
        //debug("%d", w_fd[i]);
        r_fd[i] = w_to_m_fd[0];
        //debug("%d", r_fd[i]);

        if((pid[i] = fork()) < 0) {
            perror("fork error");
            exit(1);
        }
        // WORKER/CHILD
        else if(pid[i] == 0) { // create workers/child
            debug("created new worker %d", (int)getpid());
            close(w_fd[i]); // close write side of pipe
            close(r_fd[i]); // close read side of pipe

            //(after fork) redirect stdin and stdout to pipes with dup(2)
            dup2(m_to_w_fd[0], 0); // stdin
            close(m_to_w_fd[0]);
            dup2(w_to_m_fd[1], 1); // stdout
            close(w_to_m_fd[1]);

            //execute worker program with exec(3) - stops and waits for SIGCONT
            debug("Starting worker %d", i);
            execl("bin/polya_worker", "polya_worker", NULL);
            //CATCH ERRORS // *****************
            //exit(0); // ********
        } else {
            state[i] = WORKER_STARTED;
            w_id[i] = pid[i];
            debug("Started worker %d, id = %d in = %d out = %d", i, w_id[i], w_fd[i], r_fd[i]);
            // parent
            close(m_to_w_fd[0]); // close read side of pipe
            close(w_to_m_fd[1]); // close write side of pipe
        }
    }

//debug("parent id - %d", (int)getpid());

    // main loop
    while(1) {
        //debug("parent id - %d", (int)getpid());
        //debug("ccount - %d", ccount);

        // if problems exist
        if(get_problem_variant(workers, 0)) {
            for(i = 0; i < workers; i++) {
                //debug("state = %d i = %d", state[i], i);
                if(state[i] == WORKER_IDLE){ // repeatedly assign problems to idle workers
                    struct problem* new_problem;
                    new_problem = get_problem_variant(workers, i);
                    // write header sizeof(struct problem)
                    //debug("F %d", w_fd[i]);
                    if((out = fdopen(w_fd[i], "w")) == NULL) { // write problem
                        perror("master unable to create output stream");
                        exit(1);
                    } // close *************
                    fwrite(new_problem, sizeof(struct problem), 1, out);
                    if(ferror(out)) { //ferror
                        //close
                        exit(EXIT_FAILURE);
                    }
                    // write data bytes
                    fwrite(new_problem->data, new_problem->size - sizeof(struct problem), 1, out);
                    if(ferror(out)) { //ferror
                        //close
                        exit(EXIT_FAILURE);
                    }

                    //fflush after entire problem is written
                    if(fflush(out) == EOF) {
                        perror("fflush error");
                        exit(EXIT_FAILURE);
                    }
                    //debug("W_ID %d, i = %d", w_id[i], i);
                    sf_send_problem(w_id[i], new_problem);
                    kill(SIGCONT, w_id[i]); // SIGCONT *****
                    state[i] = WORKER_CONTINUED;

                    //exit(1);

                }
    }
}


/*
if(state[i]==WORKER_IDLE){
        struct problem* new_problem;
        if ((new_problem = get_problem_variant(workers, 0)) == NULL) { // no more problems to solve
            debug("no more problems to solve");
            // terminate all workers = send SIGTERM to each worker
            kill(pid[i], SIGTERM);
            kill(pid[i], SIGCONT);
            done = 1; // terminate master process
            //exit EXIT_STATUS - outside while loop? *****
            exit(EXIT_SUCCESS); // = all workers terminated normally

        } else { // repeatedly assign problems to idle workers
            // write header sizeof(struct problem)
        }
}
*/
        /* READ RESULT
        if((in = fdopen(r_fd[i], "r")) == NULL) { // read result
            perror("master unable to create input stream");
            exit(1);
        }*/

        // repeatedly assign problems to idle workers
            // and posts results received from workers post_result
        // write fixed size problem header into pipe
            // continue to write problem data
            //**
            // until all workers become idle and NULL return from get_problem_variant() = no more problems to solve
            // send SIGTERM to each worker
        //(all workers terminated)
        // terminate master process done = 1
            //exit(EXIT_SUCCESS) = all workers terminated normally
                //exit(EXIT_FAILURE)
    }

        // reap should not crash
        // use post_result
        // send SIGTERM and SIGHUP to worker
        // fclose
        // set SIGCONT for worker
        // use event functions
        // fflush/fclose check return for errors - ferror
        //ERRORS catch
        //worker status/STATE


    sf_end(); // master process is about to terminate
    return EXIT_FAILURE;
}
