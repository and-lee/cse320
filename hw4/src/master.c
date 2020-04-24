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
int ccount = 0;
// SIGCHLD handler notifies when worker processes stop and continue
void child_handler(int sig) {
    int olderrno = errno;
    pid_t pid;
    while (ccount > 0 && (pid = waitpid(-1, NULL, WNOHANG)) != 0) {
        if(pid < 0) {
            perror("wait error");
        }
        --ccount;
        debug("reap child - handler");
    }
    if(ccount == 0) {
        done = 1;
    }
    errno = olderrno;
    debug("SIGCHLD %d", pid);
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

    // create new file objects
    int r_fd[workers];
    int w_fd[workers];

    int m_to_w_fd[2];
    int w_to_m_fd[2];
    //char buff [100];

    FILE /* *in,*/ *out;

    pid_t pid[workers];
    ccount = workers;
    int i;
    signal(SIGCHLD, child_handler); // install SIGCHLD handler
    for(i = 0; i < workers; i++) {
    //while(ccount > 0) {
        // create 2 pipes for each worker
        // use pipe(2) before worker process is forked using fork(2)
            //send problems from master to worker
            //send results from worker to master
        if((pipe(m_to_w_fd) < 0) || (pipe(w_to_m_fd) < 0)) {
            perror("unable to create pipe");
            exit(1);
        }
        w_fd[i] = m_to_w_fd[1];
        debug("%d", w_fd[i]);
        r_fd[i] = w_to_m_fd[0];
        debug("%d", r_fd[i]);

        if((pid[i] = fork()) < 0) {
            perror("fork error");
            exit(1);
        }
        // WORKER/CHILD
        else if(pid[i] == 0) { // create workers/child
            debug("created new worker %d", (int)getpid());
            close(w_fd[i]); // close write side of pipe
            close(r_fd[i]); // close read side of pipe

            /*if((out = fdopen(w_to_m_fd[1], "w")) == NULL) { // create output stream
                perror("worker unable to create output stream");
                exit(1);
            }*/
            //(after fork) redirect stdin and stdout to pipes with dup(2)
            dup2(m_to_w_fd[0], 0); // stdin
            close(m_to_w_fd[0]);
            dup2(w_to_m_fd[1], 1); // stdout
            close(w_to_m_fd[1]);

            //execute worker program with exec(3) - stops and waits for SIGCONT
            execl("bin/polya_worker", "polya_worker", NULL); // ****************
            //CATCH ERRORS // *****************
            //exit(0); // ********

        } else {
            // MASTER/PARENT
            debug("parent id - %d", (int)getpid());

            close(m_to_w_fd[0]); // close read side of pipe
            close(w_to_m_fd[1]); // close write side of pipe

            if((out = fdopen(w_fd[i], "w")) == NULL) { // write problem
                perror("master unable to create output stream");
                exit(1);
            }
            debug("W %d", w_fd[i]);

            //read/write on pipes using fread/fwrite after fdopen(3) to wrap pipe file descriptors in FILE objects

            while (!done) {
                // all workers become idle // ********************
                //
                struct problem* new_problem;
                if ((new_problem = get_problem_variant(workers, i)) == NULL) { // no more problems to solve
                    debug("no more problems to solve");
                    // terminate all workers = send SIGTERM to each worker
                    kill(pid[i], SIGTERM);
                    kill(pid[i], SIGCONT);
                    done = 1; // terminate master process
                    //exit EXIT_STATUS - outside while loop? *****
                    exit(EXIT_SUCCESS); // = all workers terminated normally

                } else { // repeatedly assign problems to idle workers
                    // write header sizeof(struct problem)
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
                    sf_send_problem(pid[i], new_problem);

                }


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
                    // until all workers become idle amd NULL return from get_problem_variant()
                        //get_problem_variant == NULL = no more problems to solve
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


        }

    }


    sf_end(); // master process is about to terminate
    return EXIT_FAILURE;
}
