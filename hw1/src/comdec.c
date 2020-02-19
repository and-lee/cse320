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
 * Converts String of digits into an int.
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
 * Helper function for setting the 16 most-significant bits
 *
 * @param ai1 int to set into the 16 most-significant bits.
 * @param ai2 int to palce ai1 into.
 * @return int value of ai2 with its 16 most-significant bits as ai1.
*/
int most_significant_bits_16(int ai1, int ai2) {
    //int bits16 = ai1;
    //int value = ai2;
    ai1 = ai1 << 16; // move to first 16 most-significant bits

    int mask = ~(1 << 15);
    ai2 = ai2 & ~(mask); // set 16 most-significant bits to 0
    ai2 = ai2 | (ai1 & mask); // insert bits into 16 most-significant bits
    return ai2;
}

/**
 * Helper function for identifying special marker bytes from UTF-8.
 * Marker bytes : SOT = 0x81, SOB = 0x83, RD = 0x85, EOB = 0x84, EOT = 0x82
 *
 * @param ai int to check.
 * @return 1-5 if byte is a marker byte and -1 if the byte is not a marker byte.
*/
int marker_byte(int ai) {
    if (ai == 0x81) {
        return 1;
    }
    if (ai == 0x83) {
        return 2;
    }
    if (ai == 0x85) {
        return 3;
    }
    if (ai == 0x84) {
        return 4;
    }
    if (ai == 0x82) {
        return 5;
    }
    return -1;
}

/**
 * Helper function for identifying header bits from byte.
 *
 * @param ai int to check.
 * @return 0-4 for corresponding headers and -1 for no usable header.
*/
int header_byte(int ai) {
    if((ai & 0x80) == 0) { // 0
        return 0;
    }
    if((ai & 0xe0) == 0xc0) { // 110
        return 1;
    }
    if((ai & 0xe0) == 0xe0) { // 111
        return 2;
    }
    if((ai & 0xf8) == 0xf0) { // 11110
        return 3;
    }
    return 0;
}

/**
 * Helper function for getting the value of the byte using its header.
 *
 * @param ai1 value.
 * @param ai2 type of header.
 * @return value of ai remaining bits without its header bits.
*/
int value_helper(int ai1, int ai2) {
    if(ai2 == 0) { // 7 bits
        return ai1; // ai1 & 0x7f
    }
    if(ai2 == 1 || ai2 == 2) { // 5 bits
        return (ai1 & 0x1f);
    }
    if(ai2 == 3) { // 3 bits
        return (ai1 & 0x7);
    }
    return -1;
}

/**
 * Helper function for getting the value a continuation byte.
 *
 * @param ai1 value.
 * @return value of ai remaining bits without its header bits or -1 if invalid value.
*/
int continuation_value(int ai) {
    if((ai & 0xc0) == 0x80) { // 6 bits
        return (ai & 0x3f);
    }
    return -1;
}

void expand(SYMBOL *as, FILE *af) {
    SYMBOL *head = as;
    as = (as -> next);
    while(as != head){
        if((as -> value) >= FIRST_NONTERMINAL) {
            /*
            if(*(rule_map + start) == NULL) {
                return -1; // undefined nonterminal
            }
            */
            expand(*(rule_map + (as -> value)), af);
        } else {
            fputc((as -> value), af);
        }
        fflush(af); //
        as = (as -> next);
    }
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
    // could not open file
    if (in == NULL || out == NULL) {
        return EOF;
    }

    int bytes_num = 0;
    int continuation = 0;
    int in_SOB = 0;
    int is_EOB = 0;
    in_SOB++;
    in_SOB--;
    is_EOB++;
    is_EOB--;
    int value = -1;
    SYMBOL *curr_sym = NULL;
    SYMBOL *rule_head = NULL;
    int symbol_count = 0;
    // read input stream byte by byte until the end of the input file
    int c;
    while ((c = fgetc(in)) != EOF) {
        // marker bytes
        if (!continuation) {
            /*
            // has to start with SOT
            if(bytes_num == 0 && (marker_byte(c) != 1)) {
                return EOF;
            }
            // SOT has to immediately be followed by SOB | EOT
            if(bytes_num == 1 && ((marker_byte(c) != 2) || (marker_byte(c) != 5))) {
                return EOF;
            }
            // EOB has to immediately be followed by SOB | EOT
            if(is_EOB && ((marker_byte(c) == -1) || (!((marker_byte(c) == 2) || (marker_byte(c) == 5))))) {
                return EOF;
            }
            // RD and EOB or symbols have to follow after SOB
            if(((marker_byte(c) == 3) || (marker_byte(c) == 4) || (marker_byte(c) == -1)) && !in_SOB) {
                return EOF;
            }

            if((marker_byte(c) == 3) || (marker_byte(c) == 4)) {
                if(symbol_count < 2) { // rule must have at least 3 symbols
                    return EOF;
                }
                symbol_count = 0;
            }
            */

            // SOB
            if(marker_byte(c) == 2) {
                init_symbols();
                init_rules();
                in_SOB = 1;
                is_EOB = 0;
            }
            // RD
            if(marker_byte(c) == 3) {
                curr_sym -> next = rule_head;
                rule_head -> prev = curr_sym;

            }

            // EOB
            if(marker_byte(c) == 4) {
                in_SOB = 1;
                is_EOB = 0;
                // EXPAND and write to output file
                //expand(main_rule, out);
                //fputc(value, out);
                //fflush(out);
                curr_sym -> next = rule_head;
                rule_head -> prev = curr_sym;
                expand(main_rule, out);

            }

            // EOT
            if(marker_byte(c) == 5) {
                // should be EOF. nothing after
                /*
                if(fgetc(in) == EOF) {
                    return EOF;
                } // else return bytes_num
                */
            }

            // non marker
            if(marker_byte(c) == -1) {
                continuation = header_byte(c); // header bits
                value = value_helper(c, continuation); // set first value
            }

            // create
            if(!continuation) {
                if(rule_head == NULL) { // create new rule
                    /*
                    // head of rule must be nonterminal
                    if(!(value >= FIRST_NONTERMINAL)) {
                        return EOF;
                    }
                    // rule with head must not already exist
                    if(*(rule_map + value) != NULL) {
                        return EOF;
                    }
                    */
                    rule_head = new_rule(value);
                    *(rule_map + value) = rule_head;
                    add_rule(rule_head);
                    curr_sym = rule_head;
                } else { // create symbol
                    SYMBOL *sym = new_symbol(value, NULL); // terminal = null. nonterminal = rule_head
                    sym -> prev = curr_sym;
                    curr_sym -> next = sym;
                    symbol_count++;
                    curr_sym = sym;
                }
            }
        }

        // calculate and concat continuation symbols
        while (continuation) {
            c = fgetc(in);
            value = (value << 6) | continuation_value(c);
            continuation--;
        }
        //bytes_num++;
        //printf("%d\n", bytes_num);
    }
    //return EOF;
    return bytes_num;
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
    // no flags = invalid
    if (argc==1) {
        global_options = 0;
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
            //16 most-significant bits = 0x0400
            int df = 0x0400;
            global_options = most_significant_bits_16(df, global_options);
            // -c : second-least-significant bit (bit1) = 1
            global_options = global_options | 1 << 1;
            return 0;
        }
        // -b tag with BLOCKSIZE
        if(argc==4) {
            char *a2 = *(argv+2);
            char *a3 = *(argv+3); // BLOCKSIZE must be digits '0-9' and range 1-1024
            int bs = string_to_int(a3);
            if(string_compare(a2,"-b")==0 && bs!=-1) {
                // BLOCKSIZE = 16 most-significant bits
                global_options = most_significant_bits_16(bs, global_options);
                // -c : second-least-significant bit (bit1) = 1
                global_options = global_options | 1 << 1;
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
    global_options = 0;
    return -1;
}
