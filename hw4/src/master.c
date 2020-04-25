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
struct problem* new_problem;

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
            state[index] = WORKER_IDLE; // set state = STARTED
        }
        if(prev_state  == WORKER_IDLE) {
            state[index] =  WORKER_CONTINUED; // set state = CONTINUED
        }
        if(prev_state  == WORKER_CONTINUED) {
            state[index] = WORKER_RUNNING; // set state = RUNNING
        }
        if(prev_state  == WORKER_RUNNING) {
            state[index] = WORKER_STOPPED; // set state = STOPPED
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

    FILE *in, *out;

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
            // block all signals ***************

            sf_change_state(w_id[i], 0, WORKER_STARTED);
            state[i] = WORKER_STARTED; // get_w_index(w_id[i])******************

            // unblock *******************

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
                    // write header sizeof(struct problem)
                    // struct problem* new_problem; *************
                    new_problem = get_problem_variant(workers, i);
                    //sf_send_problem(w_id[i], get_problem_variant(workers, i));
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
                    sf_send_problem(w_id[i], get_problem_variant(workers, i));

                    debug("W_ID %d, i = %d", w_id[i], i);
                    kill(w_id[i], SIGCONT); // SIGCONT
                    // block all signals ***************

                    sf_change_state(w_id[i], WORKER_IDLE, WORKER_CONTINUED);
                    state[get_w_index(w_id[i])] = WORKER_CONTINUED; // state[i] *********
                    // unblock *******************

                }

                if(state[i] == WORKER_STOPPED) { // worker done solving
                    // read result
                    if((in = fdopen(r_fd[i], "r")) == NULL) { // read result
                        perror("master unable to create input stream");
                        exit(1);
                    } // close ***************

                    struct result* worker_result = malloc(sizeof(struct result));
                    fread(worker_result, sizeof(struct result), 1, in);
                    // ferror
                    if(ferror(in)) {
                        exit(EXIT_FAILURE);;
                    }
                    if(fflush(in) == EOF) {
                        perror("fflush error");
                        exit(EXIT_FAILURE);
                    }

                    if(worker_result->failed == 0) { // post results if solution is not failed, = 0
                        // post result
                        post_result(worker_result, new_problem);
                        sf_recv_result(w_id[i], worker_result);

                        // if result is a valid solution, cancel other workers and move onto next problem
                        for(int k = 0; k < workers; k++) { //for rest of workers **************************************
                            kill(w_id[k], SIGHUP);
                            // change to idle
                            // block all signals ***************

                            sf_cancel(w_id[k]);
                            state[get_w_index(w_id[k])] = WORKER_IDLE; // state[k] ********* **includes itself = redundant ****
                            // unblock *******************

                        }

                    }
                    free(worker_result);
                    // change worker to idle
                    // block all signals ***************

                    sf_change_state(w_id[i], state[get_w_index(w_id[i])], WORKER_IDLE);
                    state[get_w_index(w_id[i])] = WORKER_IDLE; // state[i] *********
                    // unblock *******************


                }


            }


        } else { // get_problem_variant == NULL = no more workers
            debug("no more problems to solve");
            for(int j = 0; j < workers; j++) {
                if(state[j] == WORKER_STOPPED) {
                    // change worker to idle
                    // block all signals ***************

                    sf_change_state(w_id[j], WORKER_STOPPED, WORKER_IDLE);
                    state[j] = WORKER_IDLE; // state[i] *********
                    // unblock *******************

                }
                if(state[j] == WORKER_IDLE) { // all workers are idle
                    // terminate all workers = send SIGTERM to each worker
                    kill(w_id[j], SIGTERM);
                    kill(w_id[j], SIGCONT);
                    sf_change_state(w_id[j], state[j], WORKER_EXITED);

                }
            }
            // all children are terminated. terminate the main program
            debug("ending - success");
            sf_end(); // master process is about to terminate
            exit(EXIT_SUCCESS);
        }
    }


    // reap should not crash
    // fclose
    // use event functions
    // fflush/fclose check return for errors - ferror
    //ERRORS catch
    debug("ending - failure");
    sf_end(); // master process is about to terminate
    return EXIT_FAILURE;
}
