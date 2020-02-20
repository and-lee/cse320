#include <stdio.h>
#include <stdlib.h>

#include "const.h"
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

int main(int argc, char **argv)
{
    int ret;
    if(validargs(argc, argv))
        USAGE(*argv, EXIT_FAILURE);
    debug("Options: 0x%x", global_options);
    if(global_options & 1) // -h help
        USAGE(*argv, EXIT_SUCCESS);
    if(global_options == 0x00000004) {// -d decompress
        if(decompress(stdin, stdout) == EOF) {
            return EXIT_FAILURE;
        }
    }
    if(global_options & 2) {// -c compress
        if(compress(stdin, stdout, global_options >> 16) == EOF) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
