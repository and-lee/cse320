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
struct result* worker_result;

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
    int status;
    while((pid = waitpid(-1, &status, WSTOPPED | WCONTINUED | WNOHANG)) > 0) {
        if(pid < 0) {
            perror("wait error");
        }
        int index = get_w_index(pid);
        debug("id = %d, STATE = %d", pid, state[index]);

        int prev_state = state[index];
        // change state
        if(WIFSTOPPED(status)) {
            debug("STOPPED");
            if(prev_state == WORKER_STARTED) {
                state[index] = WORKER_IDLE; // set state = IDLE
                sf_change_state(pid, prev_state , state[index]);
            }
            else /*if(prev_state == WORKER_RUNNING)*/ {
                state[index] = WORKER_STOPPED; // set state = STOPPED
                sf_change_state(pid, prev_state , state[index]);
            }
        }
        else if(WIFCONTINUED(status)) {
            debug("CONTINUED");
            //prev_state == WORKER_CONTINUED)
            state[index] =  WORKER_RUNNING; // set state = RUNNING
            sf_change_state(pid, prev_state , state[index]);
        }
        else if(WIFEXITED(status)) {
            debug("EXITED");
            int exit_status;
            if((exit_status = WEXITSTATUS(status)) != 0){
                debug("exited normally with nonzero exit status %d", exit_status);
                state[index] = WORKER_ABORTED;
                sf_change_state(pid, prev_state , state[index]);
            } else {
                state[index] = WORKER_EXITED;
                sf_change_state(pid, prev_state , state[index]);
            }
        }
        else if(WIFSIGNALED(status)) {
            debug("ABORTED");
            // exited abnormally
            state[index] =  WORKER_ABORTED; // set state = RUNNING
            sf_change_state(pid, prev_state , state[index]);
        }

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
        perror("invalid number of workers arguement");
        exit(EXIT_FAILURE);
    }

    // ignore SIGPIPE signal
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, child_handler); // install SIGCHLD handler

    sigset_t mask;
    sigset_t prev;
    sigfillset(&mask);

    // fds for each worker
    int r_fd[workers];
    int w_fd[workers];

    int m_to_w_fd[2];
    int w_to_m_fd[2];

    // create new file objects
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
            exit(EXIT_FAILURE);
        }
        w_fd[i] = m_to_w_fd[1];
        //debug("%d", w_fd[i]);
        r_fd[i] = w_to_m_fd[0];
        //debug("%d", r_fd[i]);

        if((pid[i] = fork()) < 0) {
            perror("fork error");
            exit(EXIT_FAILURE);
        }
        // WORKER/CHILD
        else if(pid[i] == 0) { // create workers/child
            debug("created new worker %d", (int)getpid());

            //(after fork) redirect stdin and stdout to pipes with dup(2)
            dup2(m_to_w_fd[0], 0); // stdin
            close(m_to_w_fd[1]);
            dup2(w_to_m_fd[1], 1); // stdout
            close(w_to_m_fd[0]);

            //execute worker program with exec(3) - stops and waits for SIGCONT
            debug("exec worker %d", i);
            execl("bin/polya_worker", "polya_worker", NULL);


        } else {
            // parent
            close(m_to_w_fd[0]); // close read side of pipe //close(r_fd[i]); // ****************
            close(w_to_m_fd[1]); // close write side of pipe //close(w_fd[i]); // ***************

            w_id[i] = pid[i];
            // block all signals
            sigprocmask(SIG_BLOCK, &mask, &prev);
            sf_change_state(pid[i], 0, WORKER_STARTED);
            state[i] = WORKER_STARTED;
            // unblock
            sigprocmask(SIG_SETMASK, &prev, NULL);
            debug("Starting worker %d, id = %d in = %d out = %d", i, w_id[i], w_fd[i], r_fd[i]);
            sigset_t new_mask;
            sigfillset(&new_mask);
            sigdelset(&new_mask, SIGCHLD);
            // sigspend
            sigsuspend(&new_mask);

        }
    }

//debug("parent id - %d", (int)getpid());

    // main loop
    while(1) {
        // if problems exist
        if(get_problem_variant(workers, 0)) {
            for(i = 0; i < workers; i++) {
                if(state[i] == WORKER_IDLE){ // repeatedly assign problems to idle workers
                    // write header sizeof(struct problem)
                    // struct problem* new_problem; *************
                    sigprocmask(SIG_BLOCK, &mask, &prev);
                    new_problem = get_problem_variant(workers, i);
                    //debug("F %d", w_fd[i]);
                    if((out = fdopen(w_fd[i], "w")) == NULL) { // write problem
                        perror("master unable to create output stream");
                        exit(EXIT_FAILURE);
                    }
                    fwrite(new_problem, sizeof(struct problem), 1, out);
                    if(ferror(out)) { //ferror
                        //close
                        perror("out problem header ferror");
                        exit(EXIT_FAILURE);
                    }
                    new_problem = realloc(new_problem, new_problem->size);
                    // write data bytes
                    fwrite(new_problem->data, new_problem->size - sizeof(struct problem), 1, out);
                    if(ferror(out)) { //ferror
                        //close
                        perror("out problem data ferror");
                        exit(EXIT_FAILURE);
                    }
                    //fflush after entire problem is written
                    if(fflush(out) == EOF) {
                        perror("fflush error");
                        exit(EXIT_FAILURE);
                    }
                    sigprocmask(SIG_SETMASK, &prev, NULL);
                    sf_send_problem(w_id[i], get_problem_variant(workers, i));

                    debug("W_ID %d, i = %d", w_id[i], i);
                    kill(w_id[i], SIGCONT); // SIGCONT
                    // block all signals
                    sigprocmask(SIG_BLOCK, &mask, &prev);
                    sf_change_state(w_id[i], WORKER_IDLE, WORKER_CONTINUED);
                    state[i] = WORKER_CONTINUED;
                    // unblock
                    sigprocmask(SIG_SETMASK, &prev, NULL);
                    //kill(w_id[i], SIGCONT); // SIGCONT

                }

                if(state[i] == WORKER_STOPPED) { // worker done solving
                    // read result
                    sigprocmask(SIG_BLOCK, &mask, &prev);
                    if((in = fdopen(r_fd[i], "r")) == NULL) { // read result
                        perror("master unable to create input stream");
                        exit(EXIT_FAILURE);
                    }

                    worker_result = malloc(sizeof(struct result));
                    fread(worker_result, sizeof(struct result), 1, in);
                    // ferror
                    if(ferror(in)) {
                        perror("in result header ferror");
                        exit(EXIT_FAILURE);;
                    }
                    if(fflush(in) == EOF) {
                        perror("fflush error");
                        exit(EXIT_FAILURE);
                    }

                    worker_result = realloc(worker_result, worker_result->size);
                    fread(worker_result->data, worker_result->size - sizeof(struct result), 1, in);

                    sigprocmask(SIG_SETMASK, &prev, NULL);

                    sf_recv_result(w_id[i], worker_result);

                    // post result
                    if(post_result(worker_result, new_problem) == 0) { // post results if solution is not failed, = 0
                        // if result is a valid solution, cancel other workers and move onto next problem
                        for(int k = 0; k < workers; k++) {
                            if(k != i) { // cancel other workers
                                sf_cancel(w_id[k]);
                                kill(w_id[k], SIGHUP);

                                // change to idle
                                // block all signals
                                sigprocmask(SIG_BLOCK, &mask, &prev);
                                sf_change_state(w_id[k], state[k], WORKER_IDLE);
                                // unblock
                                sigprocmask(SIG_SETMASK, &prev, NULL);
                            }
                        }
                    }
                    //free(worker_result);
                    // change worker to idle
                    //kill(w_id[i], SIGSTOP); // *************************************
                    // block all signals
                    sigprocmask(SIG_BLOCK, &mask, &prev);
                    sf_change_state(w_id[i], state[i], WORKER_IDLE);
                    state[i] = WORKER_IDLE;
                    // unblock
                    sigprocmask(SIG_SETMASK, &prev, NULL);

                }
            }

        } else { // get_problem_variant == NULL = no more workers
            debug("no more problems to solve");
int c = 0;
            for(int j = 0; j < workers; j++) {
                if(state[j] == WORKER_STOPPED) {
                    // change worker to idle
                    // block all signals
                    sigprocmask(SIG_BLOCK, &mask, &prev);
                    sf_change_state(w_id[j], WORKER_STOPPED, WORKER_IDLE);
                    state[j] = WORKER_IDLE; // state[i] *********
                    // unblock
                    sigprocmask(SIG_SETMASK, &prev, NULL);

                }

                debug("worker %d - status %d", j, state[j]);

                if(state[j] == WORKER_IDLE) { // all workers are idle
                    // terminate all workers = send SIGTERM to each worker
                    debug("TERM %d", j);


//sigprocmask(SIG_BLOCK, &mask, &prev);
                    kill(w_id[j], SIGTERM);
//sigprocmask(SIG_SETMASK, &prev, NULL);


                    //sigprocmask(SIG_BLOCK, &mask, &prev);
                    kill(w_id[j], SIGCONT);
                    //sigprocmask(SIG_SETMASK, &prev, NULL);

                    sigset_t new_mask2;
                    sigfillset(&new_mask2);
                    sigdelset(&new_mask2, SIGCHLD);
                    // sigspend
                    sigsuspend(&new_mask2);
                }
                if(state[j]== WORKER_ABORTED) {
                    debug("HAS ABORT");
                    sf_end(); // master process is about to terminate
                    return EXIT_FAILURE;
                }
                if(state[j] == WORKER_EXITED) {
                    c = c+1;
                    debug("workers exited %d", c);
                }
            }

            /*sigset_t new_mask;
            sigfillset(&new_mask);
            sigdelset(&new_mask, SIGTERM);
            // sigspend
            sigsuspend(&new_mask);*/
            /*for(int o = 0; o < workers; o ++) {
                if(state[o] == WORKER_ABORTED) {
                    debug("HAS ABORT");
                    sf_end(); // master process is about to terminate
                    return EXIT_FAILURE;
                }
                if(state[o] != WORKER_EXITED) {
                    debug("NOT EXITED");
                    debug("worker after %d - status %d", o, state[o]);
                    sf_end(); // master process is about to terminate
                    return EXIT_FAILURE;
                }
            }*/
            if(c == workers) { // all children are terminated. terminate the main program
                debug("ending - success");
                //fclose(in);
                //fclose(out);

                sf_end(); // master process is about to terminate
                exit(EXIT_SUCCESS); // return EXIT_SUCCESS;
            } else {
                debug("ending - fail");

                sf_end(); // master process is about to terminate
                exit(EXIT_FAILURE);
            }

        }
    }

    // reap should not crash
    // fclose
    // use event functions
    // fflush/fclose check return for errors - ferror
    // ERRORS catch

    //sf_end(); // master process is about to terminate
    //return EXIT_FAILURE;
}
