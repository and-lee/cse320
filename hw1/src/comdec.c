#include "const.h"
#include "sequitur.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

/*
 * You may modify this file and/or move the functions contained here
 * to other source files (except for main.c) as you wish.
 *
 * IMPORTANT: You MAY NOT use any array brackets (i.e. [ and ]) and
 * you MAY NOT declare any arrays or allocate any storage with malloc().
 * The purpose of this restriction is to force you to use pointers.
 * Variables to hold the pathname of the current file or directory
 * as well as other data have been pre-declared for you in const.h.
 * You must use those variables, rather than declaring your own.
 * IF YOU VIOLATE THIS RESTRICTION, YOU WILL GET A ZERO!
 *
 * IMPORTANT: You MAY NOT use floating point arithmetic or declare
 * any "float" or "double" variables.  IF YOU VIOLATE THIS RESTRICTION,
 * YOU WILL GET A ZERO!
 */

/**
 * Compares two strings to see if they are the same.
 *
 * @param as1 String to compare.
 * @param as2 String to compare.
 * @return -1 if as1 and as2 are different of 0 if as1 and as2 are the same.
*/
int string_compare(char *as1, char *as2) {
    // same length
    while (*as1 != '\0' || *as2 != '\0') {
        if (*as1 == *as2) {
            as1++;
            as2++;
        } else {
            return -1;
        }
    }
    return 0;
}

/**
 * Converts String of digits into an int
 *
 * @param as1 String to convert.
 * @return int value of as1 or -1 if as1 was not made out of digits (0-9).
*/
int string_to_int(char *as1){
    int value = 0;
    for (int i=0; *(as1+i)!='\0'; i++) {
        char digit = *(as1+i);
        if (digit == '0' || digit == '1' || digit == '2' || digit == '3' || digit == '4'
            || digit == '5' || digit == '6' || digit == '7' || digit == '8' || digit == '9') {
            int num = digit - '0';
            value = value * 10 + num;
        } else {
            return -1; // has to be digits only
        }
    }
    return value;
}

/**
 * Main compression function.
 * Reads a sequence of bytes from a specified input stream, segments the
 * input data into blocks of a specified maximum number of bytes,
 * uses the Sequitur algorithm to compress each block of input to a list
 * of rules, and outputs the resulting compressed data transmission to a
 * specified output stream in the format detailed in the header files and
 * assignment handout.  The output stream is flushed once the transmission
 * is complete.
 *
 * The maximum number of bytes of uncompressed data represented by each
 * block of the compressed transmission is limited to the specified value
 * "bsize".  Each compressed block except for the last one represents exactly
 * "bsize" bytes of uncompressed data and the last compressed block represents
 * at most "bsize" bytes.
 *
 * @param in  The stream from which input is to be read.
 * @param out  The stream to which the block is to be written.
 * @param bsize  The maximum number of bytes read per block.
 * @return  The number of bytes written, in case of success,
 * otherwise EOF.
 */
int compress(FILE *in, FILE *out, int bsize) {
    // To be implemented.
    return EOF;
}

/**
 * Main decompression function.
 * Reads a compressed data transmission from an input stream, expands it,
 * and and writes the resulting decompressed data to an output stream.
 * The output stream is flushed once writing is complete.
 *
 * @param in  The stream from which the compressed block is to be read.
 * @param out  The stream to which the uncompressed data is to be written.
 * @return  The number of bytes written, in case of success, otherwise EOF.
 */
int decompress(FILE *in, FILE *out) {
    // To be implemented.
    return EOF;
}

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the selected program options will be set in the
 * global variable "global_options", where they will be accessible
 * elsewhere in the program.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * Refer to the homework document for the effects of this function on
 * global variables.
 * @modifies global variable "global_options" to contain a bitmap representing
 * the selected options.
 */
int validargs(int argc, char **argv) {
    global_options = 0;
    // no flags = invalid
    if (argc==1) {
        //global_options = 0;
        return -1;
    }

    //*(argv+0) = "bin/sequitur"
    // First Flag
    char *a1 = *(argv+1);
    // -h
    if(string_compare(a1,"-h")==0) {
        // -h : least significant bit (bit0) = 1
        global_options = global_options | 1 << 0; // int = 32 bits
        // ignore rest
        return 0; // success if has -h flag
    }
    // -c
    if (string_compare(a1,"-c")==0) {
        // just -c tag
        if(argc==2) {
            // -c : second-least-significant bit (bit1) = 1
            global_options = global_options | 1 << 1;
            //16 most-significant bits = 0x0400
            int df = 0x04000000;
            int mask = ~((1 << 15) << 0);
            global_options &= ~(mask << 0); // set 16 most-significant bits to 0
            global_options |= (df & mask) << 0; // insert deafualt value (0x0400) into 16 most-significant bits
            return 0;
        }
        // -b tag with BLOCKSIZE
        if(argc==4) {
            char *a2 = *(argv+2);
            char *a3 = *(argv+3); // BLOCKSIZE must be digits '0-9' and range 1-1024
            int bs = string_to_int(a3);
            if(string_compare(a2,"-b")==0 && bs!=-1) {
                // -c : second-least-significant bit (bit1) = 1
                global_options = global_options | 1 << 1;
                // BLOCKSIZE = 16 most-significant bits
                int mask = ~((1 << 15) << 0);
                global_options &= ~(mask << 0); // set 16 most-significant bits to 0
                global_options |= (bs & mask) << 0; // insert BLOCKSIZE into 16 most-significant bits
                return 0;
            }
        } // too many/few arguments
    }
    // -d && no more arguements afterwards
    if (string_compare(a1,"-d")==0 && argc==2) {
        // -d : third-least-significant bit (bit2) = 1
        global_options = global_options | 0x00000004;
        return 0;
    }
    // not -h|-c|-d
    //global_options = 0;
    return -1;
}
