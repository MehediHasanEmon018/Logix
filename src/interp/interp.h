/* Logix TAC interpreter: executes the generated Three Address Code and prints the program's output */
#ifndef LOGIX_INTERP_H
#define LOGIX_INTERP_H
#include <stdio.h>

/* Reads TAC text from 'tac', runs it, program output goes to stdout.
 * Returns 0 on success, 1 on a runtime error (message already printed to stderr). */
int interp_run(FILE *tac);

#endif