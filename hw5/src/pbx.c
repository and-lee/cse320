#include "pbx.h"
#include "server.h"
#include "debug.h"
#include <stdio.h>

#include "csapp.h"

struct tu { //typedef struct tu TU;
    struct tu *peer;
    TU_STATE state; // str
    int fd; // file descriptor
    // int extension_number; // fd = extension number
    FILE *out;
};

struct pbx { //typedef struct pbx PBX;
    struct tu *clients[PBX_MAX_EXTENSIONS];
};

/* typedef enum tu_state {
    TU_ON_HOOK, TU_RINGING, TU_DIAL_TONE, TU_RING_BACK, TU_BUSY_SIGNAL,
    TU_CONNECTED, TU_ERROR
} TU_STATE;*/
//extern char *tu_state_names[];

/*
 * Initialize a new PBX.
 *
 * @return the newly initialized PBX, or NULL if initialization fails.
 */
PBX *pbx_init() {
    // mutex **************

    struct pbx *PBX = malloc(sizeof(struct pbx)); // malloc(sizeof(struct tu) * PBX_MAX_EXTENSIONS);
    if(PBX) {
        for(int i = 0; i<PBX_MAX_EXTENSIONS; i++) {
            PBX->clients[i] = NULL; // init
        }
    }
    return PBX;
}

/*
 * Shut down a pbx, shutting down all network connections, waiting for all server
 * threads to terminate, and freeing all associated resources.
 * If there are any registered extensions, the associated network connections are
 * shut down, which will cause the server threads to terminate.
 * Once all the server threads have terminated, any remaining resources associated
 * with the PBX are freed.  The PBX object itself is freed, and should not be used again.
 *
 * @param pbx  The PBX to be shut down.
 */
void pbx_shutdown(PBX *pbx) {

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
    // mutex **************

    //add to pbx->clients
    struct tu *TU = malloc(sizeof(struct tu)); // free in unregister
    if(TU && pbx->clients[fd] == NULL) {
        TU->fd = fd;
        TU->state = TU_ON_HOOK;
        pbx->clients[fd] = TU;
        // notification of assigned extension number sent to network
        FILE *out;
        if((out = fdopen(fd, "w")) == NULL) { // write problem
            perror("unable to create output stream");
            exit(EXIT_FAILURE);
        }
        TU->out = out;
        // ON HOOK <fd>
        if(fprintf(TU->out, "%s %d\n", tu_state_names[TU->state], fd) < 0) {
            perror("fprintf error");
            exit(EXIT_FAILURE);
        }
        //fflush after
        if(fflush(TU->out) == EOF) {
            perror("fflush error");
            exit(EXIT_FAILURE);
        }
        return TU;
    }
    return NULL; // registration failed
}

/*
 * This object is freed as a result of the call and must not be used again.
 * @return 0 if unregistration succeeds, otherwise -1.
 */
int pbx_unregister(PBX *pbx, TU *tu) {
    if(tu) { // ptr exists - going to be freed
        fclose(tu->out); // ***************** shutdown
        free(tu);
        return 0; // successful
    }
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
    // mutex **************

    if(tu->state == TU_ON_HOOK) {
        tu->state = TU_DIAL_TONE; // on hook->dial tone
    }

    // mutex 2 **************
    else if(tu->state == TU_RINGING) {
        tu->state = TU_CONNECTED; // ringing->connected
        if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->peer->fd) < 0) {
            perror("fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->out) == EOF) {
            perror("fflush error");
            return -1;
        }
        // calling TU also transistions to connected
        tu->peer->state = TU_CONNECTED;
        // notification of calling TU
        if(fprintf(tu->peer->out, "%s %d\n", tu_state_names[tu->peer->state], tu->fd) < 0) {
            perror("fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->peer->out) == EOF) {
            perror("fflush error");
            return -1;
        }
        return 0;
    }

    // any other state = remains in that state
    // notification of new state
    if(fprintf(tu->out, "%s\n", tu_state_names[tu->state]) < 0) {
        perror("fprintf error");
        return -1;
    }
    //fflush after
    if(fflush(tu->out) == EOF) {
        perror("fflush error");
        return -1;
    }

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
    // mutex **************

    // mutex 2 **************
    if(tu->state == TU_CONNECTED) {
        tu->state = TU_ON_HOOK; // connected->on hook
        // *
        if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->fd) < 0) {
            perror("fprintf error");
            exit(EXIT_FAILURE);
        }
        //fflush after
        if(fflush(tu->out) == EOF) {
            perror("fflush error");
            exit(EXIT_FAILURE);
        }
        // peer TU -> dial tone
        tu->peer->state = TU_DIAL_TONE; // ->dialtone
        // notification of peer TU
        if(fprintf(tu->peer->out, "%s \n", tu_state_names[tu->peer->state]) < 0) {
            perror("fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->peer->out) == EOF) {
            perror("fflush error");
            return -1;
        }
    }
    else if(tu->state == TU_RING_BACK) {
        tu->state = TU_ON_HOOK; // connected->on hook
        // *
        if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->fd) < 0) {
            perror("fprintf error");
            exit(EXIT_FAILURE);
        }
        //fflush after
        if(fflush(tu->out) == EOF) {
            perror("fflush error");
            exit(EXIT_FAILURE);
        }
        // peer TU -> dial tone
        tu->peer->state = TU_ON_HOOK; // ringing->on hook
        // *
        if(fprintf(tu->peer->out, "%s %d\n", tu_state_names[tu->peer->state], tu->peer->fd) < 0) {
            perror("fprintf error");
            exit(EXIT_FAILURE);
        }
        //fflush after
        if(fflush(tu->peer->out) == EOF) {
            perror("fflush error");
            exit(EXIT_FAILURE);
        }
    }
    else if(tu->state == TU_RINGING) {
        tu->state = TU_ON_HOOK; // connected->on hook
        // *
        if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->fd) < 0) {
            perror("fprintf error");
            exit(EXIT_FAILURE);
        }
        //fflush after
        if(fflush(tu->out) == EOF) {
            perror("fflush error");
            exit(EXIT_FAILURE);
        }
        // peer TU -> dial tone
        tu->peer->state = TU_DIAL_TONE; // ring back->dial tone
        // notification of peer TU
        if(fprintf(tu->peer->out, "%s \n", tu_state_names[tu->peer->state]) < 0) {
            perror("fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->peer->out) == EOF) {
            perror("fflush error");
            return -1;
        }
    }
    else if(tu->state == TU_DIAL_TONE || tu->state == TU_BUSY_SIGNAL || tu->state == TU_ERROR) {
        tu->state = TU_ON_HOOK; // connected->on hook
        // *
        if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->fd) < 0) {
            perror("fprintf error");
            exit(EXIT_FAILURE);
        }
        //fflush after
        if(fflush(tu->out) == EOF) {
            perror("fflush error");
            exit(EXIT_FAILURE);
        }
    }
    else {
        // no change for rest of states
        // *
        if(tu->state == TU_ON_HOOK) {
            if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->fd) < 0) {
            perror("fprintf error");
            exit(EXIT_FAILURE);
            }
            //fflush after
            if(fflush(tu->out) == EOF) {
                perror("fflush error");
            }
        }
        else {
            // notification of new state
            if(fprintf(tu->out, "%s\n", tu_state_names[tu->state]) < 0) {
                perror("fprintf error");
                return -1;
            }
            //fflush after
            if(fflush(tu->out) == EOF) {
                perror("fflush error");
                return -1;
            }
        }

    }

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
    // mutex **************

    struct tu *dialed = pbx->clients[ext];
    if(dialed == NULL) {
        tu->state = TU_ERROR;
    }
    else if(tu->state == TU_DIAL_TONE) {
        // mutex 2 **************

        if(dialed->state == TU_ON_HOOK) {
            tu->state = TU_RING_BACK;
            tu->peer = dialed;

            dialed->state = TU_RINGING;
            dialed->peer = tu;
            // notification of peer TU
            if(fprintf(tu->peer->out, "%s \n", tu_state_names[tu->peer->state]) < 0) {
                perror("fprintf error");
                return -1;
            }
            //fflush after
            if(fflush(tu->peer->out) == EOF) {
                perror("fflush error");
                return -1;
            }
        }

        else if(dialed->state != TU_ON_HOOK) {
            tu->state = TU_BUSY_SIGNAL;
        }
    }
    //else if(tu->state != TU_DIAL_TONE) {
    // no state change
    // notification of new state
    if(fprintf(tu->out, "%s\n", tu_state_names[tu->state]) < 0) {
        perror("fprintf error");
        return -1;
    }
    //fflush after
    if(fflush(tu->out) == EOF) {
        perror("fflush error");
        return -1;
    }

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
    // mutex **************

    if(tu->state != TU_CONNECTED) {
        return -1; // nothing is sent
    } else {
    // mutex 2 **************

        // send message
        // tu->peer->out CHAT msg
        if(fprintf(tu->peer->out, "CHAT %s\n", msg) < 0) {
            perror("fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->peer->out) == EOF) {
            perror("fflush error");
            return -1;
        }

        // tu that just sent chat : (chat msg), connected peer#
        if(fprintf(tu->out, "%s %d\n", tu_state_names[tu->state], tu->peer->fd) < 0) {
            perror("fprintf error");
            return -1;
        }
        //fflush after
        if(fflush(tu->out) == EOF) {
            perror("fflush error");
            return -1;
        }

    }
    return 0;
}

