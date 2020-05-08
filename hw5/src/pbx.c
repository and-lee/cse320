#include "pbx.h"
#include "server.h"
#include "debug.h"
#include <stdio.h>
#include <semaphore.h>
#include "csapp.h"

struct tu { //typedef struct tu TU;
    struct tu *peer;
    TU_STATE state; // str
    int fd; // file descriptor
    // int extension_number; // fd = extension number
    FILE *out;
    sem_t mutex;

};

struct pbx { //typedef struct pbx PBX;
    struct tu *clients[PBX_MAX_EXTENSIONS];
    sem_t mutex;
};

/*
 * Initialize a new PBX.
 *
 * @return the newly initialized PBX, or NULL if initialization fails.
 */
PBX *pbx_init() {
    struct pbx *PBX = malloc(sizeof(struct pbx)); // malloc(sizeof(struct tu) * PBX_MAX_EXTENSIONS);
    if(PBX) {
        Sem_init(&PBX->mutex, 0 ,1); // mutex = 1
        P(&PBX->mutex);
        for(int i = 0; i<PBX_MAX_EXTENSIONS; i++) {
            PBX->clients[i] = NULL; // init
        }
        V(&PBX->mutex);
    }
    return PBX;
}

/*
 * Shut down a pbx, shutting down all network connections, waiting for all server
 * threads to terminate, and freeing all associated resources.
 * If there are any registered extensions, the associated network connections are
 * shut down, which will cause the server threads to terminate.
 * Once all the server threads have terminated, any remaining resources associated
 * with the PBX are freed. The PBX object itself is freed, and should not be used again.
 */
void pbx_shutdown(PBX *pbx) {

    // shutdown(2) socket rd/wr

    // wait for client service threads to unregister - semaphore

    free(pbx);
}

/*
 * Register a TU client with a PBX.
 * The TU is assigned an extension number and it is initialized to the TU_ON_HOOK state.
 * A notification of the assigned extension number is sent to the underlying network
 * client.
 *
 * @return A TU object representing the client TU, if registration succeeds, otherwise NULL.
 * The caller is responsible for eventually calling pbx_unregister to free the TU object
 * that was returned.
 */
TU *pbx_register(PBX *pbx, int fd) {
debug("resgister %d", fd);
P(&pbx->mutex);
    //add to pbx->clients
    struct tu *TU = malloc(sizeof(struct tu)); // free in unregister
    if(TU && pbx->clients[fd] == NULL) {
        Sem_init(&TU->mutex, 0 ,1); // mutex = 1
        P(&TU->mutex);
        TU->fd = fd;
        TU->state = TU_ON_HOOK;
        //P(&pbx->mutex);
        pbx->clients[fd] = TU;
        //V(&pbx->mutex);

        // notification of assigned extension number sent to network
        FILE *out;
        if((out = fdopen(fd, "w")) == NULL) { // write problem
            perror("register unable to create output stream");
            exit(EXIT_FAILURE);
        }
        TU->out = out;
        // ON HOOK <fd>
        if(fprintf(TU->out, "%s %d\n", tu_state_names[TU->state], fd) < 0) {
            perror("register fprintf error");
            exit(EXIT_FAILURE);
        }
        //fflush after
        if(fflush(TU->out) == EOF) {
            perror("register fflush error");
            exit(EXIT_FAILURE);
        }
        V(&TU->mutex);
        V(&pbx->mutex);
        return TU;
    }
    V(&pbx->mutex);
    return NULL; // registration failed
}

/*
 * This object is freed as a result of the call and must not be used again.
 * @return 0 if unregistration succeeds, otherwise -1.
 */
int pbx_unregister(PBX *pbx, TU *tu) {
debug("unresgister %d", tu->fd);
P(&pbx->mutex);
P(&tu->mutex);
    if(tu) { // ptr exists - going to be freed
        //P(&pbx->mutex);
        //pbx->clients[tu->fd] = NULL;
        //V(&pbx->mutex);

        //P(&tu->mutex);
        if(tu->peer) {
            struct tu* p = tu->peer;

            V(&tu->mutex);
            // block smaller fd first
            if(tu->fd < p->fd) { // fd < peer fd
                P(&tu->mutex);
                P(&p->mutex);
            } else {
                P(&p->mutex);
                P(&tu->mutex);
            }
            // lock both tus
            // check if they are still peers
            if(tu->peer->peer != tu && tu->peer != p) {
                // if not peers, unlock both and go back and start over
                V(&tu->mutex);
                V(&p->mutex);
                pbx_unregister(pbx, tu);
            }
            if(p->peer == tu) {
                debug("x %d", p->fd);
                p->peer = NULL;
                p->state = TU_DIAL_TONE;

                // no change for rest of states
                // *
                if(tu->state == TU_ON_HOOK) {
                    debug("e");

                    if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->fd) < 0) {
                        perror("hangup tu out fprintf error");
                        return -1;
                    }
                    //fflush after
                    if(fflush(tu->out) == EOF) {
                        perror("5hangup tu out fflush error");
                        return -1;
                    }
                }
                else {
                    debug("f");

                    // notification of new state
                    if(fprintf(tu->out, "%s\n", tu_state_names[tu->state]) < 0) {
                        perror("hangup tu out fprintf error");
                        return -1;
                    }
                    //fflush after
                    if(fflush(tu->out) == EOF) {
                        perror("6hangup tu out fflush error");
                        return -1;
                    }
                }
            }

            V(&tu->mutex);
            V(&p->mutex);

            pbx->clients[tu->fd] = NULL;
            free(tu);
            V(&pbx->mutex);
            return 0; // successful

        }
        V(&tu->mutex);

        pbx->clients[tu->fd] = NULL;
        free(tu);
        V(&pbx->mutex);
        return 0; // successful
    }
    V(&pbx->mutex);
    V(&tu->mutex);
    return -1;
}

/*
 * Get the file descriptor for the network connection underlying a TU.
 * This file descriptor should only be used by a server to read input from
 * the connection.  Output to the connection must only be performed within
 * the PBX functions.
 *
 * @return the underlying file descriptor, if any, otherwise -1.
 */
int tu_fileno(TU *tu) {
    if(tu->fd) { // if fd exists
        return tu->fd;
    }
    return -1; // can init tu fd = -1 *
}

/*
 * Get the extension number for a TU.
 * This extension number is assigned by the PBX when a TU is registered
 * and it is used to identify a particular TU in calls to tu_dial().
 * The value returned might be the same as the value returned by tu_fileno(),
 * but is not necessarily so.
 *
 * @return the extension number, if any, otherwise -1.
 */
int tu_extension(TU *tu) {
    if(tu->fd) {
        return tu->fd;
    }
    return -1;
}

/*
 *   If the TU was in the TU_ON_HOOK state, it goes to the TU_DIAL_TONE state.
 *   If the TU was in the TU_RINGING state, it goes to the TU_CONNECTED state,
 *     reflecting an answered call.  In this case, the calling TU simultaneously
 *     also transitions to the TU_CONNECTED state.
 *   If the TU was in any other state, then it remains in that state.
 *
 * In all cases, a notification of the new state is sent to the network client
 * underlying this TU.
 *
 * @return 0 if successful, -1 if any error occurs.
 */
int tu_pickup(TU *tu) {
debug("pickup %d", tu->fd);
    // mutex
    P(&tu->mutex);

    if(tu->state == TU_ON_HOOK) {
        tu->state = TU_DIAL_TONE; // on hook->dial tone
    }
    // mutex 2
    else if(tu->state == TU_RINGING && tu->peer) {
        struct tu* p = tu->peer;
        V(&tu->mutex);

        // block smaller fd first
        if(tu->fd < p->fd) { // fd < peer fd
            P(&tu->mutex);
            P(&p->mutex);
        } else {
            P(&p->mutex);
            P(&tu->mutex);
        }
        // lock both tus
        // check if they are still peers
        if(tu->peer->peer != tu && tu->peer != p) {
            // if not peers, unlock both and go back and start over
            V(&tu->mutex);
            V(&p->mutex);
            tu_pickup(tu);
        }

        tu->state = TU_CONNECTED; // ringing->connected
        // calling TU also transistions to connected
        tu->peer->state = TU_CONNECTED;

        if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->peer->fd) < 0) {
            perror("pickup tu out fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->out) == EOF) {
            perror("1pickup tu out fflush error");
            return -1;
        }

        // notification of calling TU
        if(fprintf(tu->peer->out, "%s %d\n", tu_state_names[tu->peer->state], tu->fd) < 0) {
            perror("pickup tu peer out fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->peer->out) == EOF) {
            perror("pickup tu peer out fflush error");
            return -1;
        }

        V(&tu->mutex);
        V(&p->mutex);
        return 0;
    }

    // any other state = remains in that state
    // notification of new state
    if(fprintf(tu->out, "%s\n", tu_state_names[tu->state]) < 0) {
        perror("pickup tu out fprintf error");
        return -1;
    }
    //fflush after
    if(fflush(tu->out) == EOF) {
        perror("2pickup tu out fflush error");
        return -1;
    }
    V(&tu->mutex);
    return 0;
}

/*
 *   If the TU was in the TU_CONNECTED state, then it goes to the TU_ON_HOOK state.
 *     In addition, in this case the peer TU (the one to which the call is currently
 *     connected) simultaneously transitions to the TU_DIAL_TONE state.
 *   If the TU was in the TU_RING_BACK state, then it goes to the TU_ON_HOOK state.
 *     In addition, in this case the calling TU (which is in the TU_RINGING state)
 *     simultaneously transitions to the TU_ON_HOOK state.
 *   If the TU was in the TU_RINGING state, then it goes to the TU_ON_HOOK state.
 *     In addition, in this case the called TU (which is in the TU_RING_BACK state)
 *     simultaneously transitions to the TU_DIAL_TONE state.
 *   If the TU was in the TU_DIAL_TONE, TU_BUSY_SIGNAL, or TU_ERROR state,
 *     then it goes to the TU_ON_HOOK state.
 *   If the TU was in any other state, then there is no change of state.
 *
 * In all cases, a notification of the new state is sent to the network client
 * underlying this TU.
 *
 * @return 0 if successful, -1 if any error occurs.  Note that "error" refers to
 * an underlying I/O or other implementation error; a transition to the TU_ERROR
 * state (with no underlying implementation error) is considered a normal occurrence
 * and would result in 0 being returned.
 */
int tu_hangup(TU *tu) {
debug("hangup %d", tu->fd);
    // mutex
    P(&tu->mutex);

    if(tu->state == TU_CONNECTED && tu->peer) {
        debug("a");

        struct tu* p = tu->peer;
        // mutex 2
        V(&tu->mutex);
        // block smaller fd first
        if(tu->fd < p->fd) { // fd < peer fd
            P(&tu->mutex);
            P(&p->mutex);
        } else {
            P(&p->mutex);
            P(&tu->mutex);
        }
        // lock both tus
        // check if they are still peers
        if(tu->peer->peer != tu && tu->peer != p) {
            // if not peers, unlock both and go back and start over
            V(&tu->mutex);
            V(&p->mutex);
            tu_hangup(tu);
        }

        tu->state = TU_ON_HOOK; // connected->on hook
        // peer TU -> dial tone
        tu->peer->state = TU_DIAL_TONE; // ->dialtone

        if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->fd) < 0) {
            perror("hangup tu out fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->out) == EOF) {
            perror("1hangup tu out fflush error");
            return -1;
        }

        // notification of peer TU
        if(fprintf(tu->peer->out, "%s \n", tu_state_names[tu->peer->state]) < 0) {
            perror("hangup tu peer out fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->peer->out) == EOF) {
            perror("hangup tu peer out fflush error");
            return -1;
        }

        p->peer = NULL;
        tu->peer = NULL;
        V(&tu->mutex);
        V(&p->mutex);
        return 0;
    }
    else if(tu->state == TU_RING_BACK && tu->peer) {
        debug("b");

        struct tu* p = tu->peer;
        // mutex 2
        V(&tu->mutex);
        // block smaller fd first
        if(tu->fd < p->fd) { // fd < peer fd
            P(&tu->mutex);
            P(&p->mutex);
        } else {
            P(&p->mutex);
            P(&tu->mutex);
        }
        // lock both tus
        // check if they are still peers
        if(tu->peer->peer != tu && tu->peer != p) {
            // if not peers, unlock both and go back and start over
            V(&tu->mutex);
            V(&p->mutex);
            tu_hangup(tu);
        }

        tu->state = TU_ON_HOOK; // connected->on hook
        // peer TU -> dial tone
        tu->peer->state = TU_ON_HOOK; // ringing->on hook

        if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->fd) < 0) {
            perror("hangup tu out fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->out) == EOF) {
            perror("2hangup tu out fflush error");
            return -1;
        }

        if(fprintf(tu->peer->out, "%s %d\n", tu_state_names[tu->peer->state], tu->peer->fd) < 0) {
            perror("hangup tu peer out fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->peer->out) == EOF) {
            perror("hangup tu peer out fflush error");
            return -1;
        }

        p->peer = NULL;
        tu->peer = NULL;
        V(&tu->mutex);
        V(&p->mutex);
        return 0;
    }
    else if(tu->state == TU_RINGING && tu->peer) {
        debug("c");

        struct tu* p = tu->peer;
        // mutex 2
        V(&tu->mutex);
        // block smaller fd first
        if(tu->fd < p->fd) { // fd < peer fd
            P(&tu->mutex);
            P(&p->mutex);
        } else {
            P(&p->mutex);
            P(&tu->mutex);
        }
        // lock both tus
        // check if they are still peers
        if(tu->peer->peer != tu && tu->peer != p) {
            // if not peers, unlock both and go back and start over
            V(&tu->mutex);
            V(&p->mutex);
            tu_hangup(tu);
        }

        tu->state = TU_ON_HOOK; // connected->on hook
        // peer TU -> dial tone
        tu->peer->state = TU_DIAL_TONE; // ring back->dial tone

        if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->fd) < 0) {
            perror("hangup tu out fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->out) == EOF) {
            perror("3hangup tu out fflush error");
            return -1;
        }

        // notification of peer TU
        if(fprintf(tu->peer->out, "%s \n", tu_state_names[tu->peer->state]) < 0) {
            perror("hangup tu peer out fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->peer->out) == EOF) {
            perror("hangup tu peer out fflush error");
            return -1;
        }

        p->peer = NULL;
        tu->peer = NULL;
        V(&tu->mutex);
        V(&p->mutex);
        return 0;
    }
    else if(tu->state == TU_DIAL_TONE || tu->state == TU_BUSY_SIGNAL || tu->state == TU_ERROR) {
        debug("d");

        tu->state = TU_ON_HOOK; // connected->on hook
        // *
        if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->fd) < 0) {
            perror("hangup tu out fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->out) == EOF) {
            perror("4hangup tu out fflush error");
            return -1;
        }
    }
    else {
        // no change for rest of states
        // *
        if(tu->state == TU_ON_HOOK) {
            debug("e");

            if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->fd) < 0) {
                perror("hangup tu out fprintf error");
                return -1;
            }
            //fflush after
            if(fflush(tu->out) == EOF) {
                perror("5hangup tu out fflush error");
                return -1;
            }
        }
        else {
            debug("f");

            // notification of new state
            if(fprintf(tu->out, "%s\n", tu_state_names[tu->state]) < 0) {
                perror("hangup tu out fprintf error");
                return -1;
            }
            //fflush after
            if(fflush(tu->out) == EOF) {
                perror("6hangup tu out fflush error");
                return -1;
            }
        }

    }
debug("g");

    V(&tu->mutex);
    return 0;
}

/*
 *   If the specified extension number does not refer to any currently registered
 *     extension, then the TU transitions to the TU_ERROR state.
 *   Otherwise, if the TU was in the TU_DIAL_TONE state, then what happens depends
 *     on the current state of the dialed extension:
 *       If the dialed extension was in the TU_ON_HOOK state, then the calling TU
 *         transitions to the TU_RING_BACK state and the dialed TU simultaneously
 *         transitions to the TU_RINGING state.
 *       If the dialed extension was not in the TU_ON_HOOK state, then the calling
 *         TU transitions to the TU_BUSY_SIGNAL state and there is no change to the
 *         state of the dialed extension.
 *   If the TU was in any state other than TU_DIAL_TONE, then there is no state change.
 *
 * In all cases, a notification of the new state is sent to the network client
 * underlying this TU.
 *
 * @return 0 if successful, -1 if any error occurs.  Note that "error" refers to
 * an underlying I/O or other implementation error; a transition to the TU_ERROR
 * state (with no underlying implementation error) is considered a normal occurrence
 * and would result in 0 being returned.
 */
int tu_dial(TU *tu, int ext) {
debug("dial %d", tu->fd);
    // mutex
    P(&tu->mutex);

    if(tu->state == TU_ON_HOOK) {
        // cannot dial when on hook
    } else {
        struct tu *dialed = pbx->clients[ext]; // **** out of bounds error **************
        if(dialed == NULL) {
            tu->state = TU_ERROR;
        }
        else if(tu->state == TU_DIAL_TONE) {

            if(dialed->state == TU_ON_HOOK) {
                tu->peer = dialed;

                // mutex 2
                V(&tu->mutex);
                // block smaller fd first
                if(tu->fd < tu->peer->fd) { // fd < peer fd
                    P(&tu->mutex);
                    P(&tu->peer->mutex);
                } else {
                    P(&tu->peer->mutex);
                    P(&tu->mutex);
                }
                // lock both tus
                // check if they are still peers
                if(tu->peer->peer != tu && tu->peer != dialed) {
                    // if not peers, unlock both and go back and start over
                    V(&tu->mutex);
                    V(&dialed->mutex);
                    tu_dial(tu, ext);
                }

                tu->state = TU_RING_BACK;
                //tu->peer = dialed;
                if(fprintf(tu->out, "%s\n", tu_state_names[tu->state]) < 0) {
                    perror("dial tu out fprintf error");
                    return -1;
                }
                //fflush after
                if(fflush(tu->out) == EOF) {
                    perror("1dial tu out fflush error");
                    return -1;
                }

                dialed->state = TU_RINGING;
                dialed->peer = tu;
                // notification of peer TU
                if(fprintf(tu->peer->out, "%s \n", tu_state_names[tu->peer->state]) < 0) {
                    perror("dial tu peer out fprintf error");
                    return -1;
                }
                //fflush after
                if(fflush(tu->peer->out) == EOF) {
                    perror("dial tu peer out fflush error");
                    return -1;
                }

                V(&tu->mutex);
                V(&tu->peer->mutex);
                return 0;
            }

            else if(dialed->state != TU_ON_HOOK) {
                tu->state = TU_BUSY_SIGNAL;
            }
        }
    }

    //else if(tu->state != TU_DIAL_TONE) {
    // no state change
    // notification of new state
    if(fprintf(tu->out, "%s\n", tu_state_names[tu->state]) < 0) {
        perror("dial tu out fprintf error");
        return -1;
    }
    //fflush after
    if(fflush(tu->out) == EOF) {
        perror("dial tu out fflush error");
        return -1;
    }

    V(&tu->mutex);
    return 0;
}

/*
 * If the state of the TU is not TU_CONNECTED, then nothing is sent and -1 is returned.
 * Otherwise, the specified message is sent via the network connection to the peer TU.
 * In all cases, the states of the TUs are left unchanged.
 *
 * @return 0  If the chat was successfully sent, -1 if there is no call in progress
 * or some other error occurs.
 */
int tu_chat(TU *tu, char *msg) {
debug("chat %d", tu->fd);
    // mutex
    P(&tu->mutex);

    if(tu->state != TU_CONNECTED) {
        if(tu->state == TU_ON_HOOK) {
            debug("1");
            if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->fd) < 0) {
                perror("chat tu out fprintf error");
                return -1;
            }
            //fflush after
            if(fflush(tu->out) == EOF) {
                perror("chat tu out fflush error");
                return -1;
            }
        }
        else {
            debug("2");
            // notification of new state
            if(fprintf(tu->out, "%s\n", tu_state_names[tu->state]) < 0) {
                perror("chat tu out fprintf error");
                return -1;
            }
            //fflush after
            if(fflush(tu->out) == EOF) {
                perror("chat tu out fflush error");
                return -1;
            }
        }

        V(&tu->mutex);
        return -1; // nothing is sent
    } else if(tu->peer) {
        debug("3");
        struct tu* p = tu->peer;
        // mutex 2
        V(&tu->mutex);

        // block smaller fd first
        if(tu->fd < p->fd) { // fd < peer fd
            P(&tu->mutex);
            P(&p->mutex);
        } else {
            P(&p->mutex);
            P(&tu->mutex);
        }
        // lock both tus
        // check if they are still peers
        if(tu->peer->peer != tu && tu->peer != p) {
            // if not peers, unlock both and go back and start over
            V(&tu->mutex);
            V(&p->mutex);
            tu_chat(tu, msg);
        }


        // send message
        // tu->peer->out CHAT msg
        if(fprintf(tu->peer->out, "CHAT %s\n", msg) < 0) {
            perror("chat tu peer out fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->peer->out) == EOF) {
            perror("chat tu peer out fflush error");
            return -1;
        }

        // tu that just sent chat : (chat msg), connected peer#
        if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->peer->fd) < 0) {
            perror("chat tu out fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->out) == EOF) {
            perror("chat tu out fflush error");
            return -1;
        }

        V(&tu->mutex);
        V(&tu->peer->mutex);
        return 0;
    }


    // no change for rest of states
    // *
    if(tu->state == TU_ON_HOOK) {
        debug("e");

        if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->fd) < 0) {
            perror("hangup tu out fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->out) == EOF) {
            perror("5hangup tu out fflush error");
            return -1;
        }
    }
    else {
        debug("f");

        // notification of new state
        if(fprintf(tu->out, "%s\n", tu_state_names[tu->state]) < 0) {
            perror("hangup tu out fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->out) == EOF) {
            perror("6hangup tu out fflush error");
            return -1;
        }
    }
    V(&tu->mutex);
    return 0;
}
