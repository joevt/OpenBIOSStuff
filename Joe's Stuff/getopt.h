#ifndef __GETOPT__
#define __GETOPT__

/* Some systems seem to have an incomplete unistd.h.
 * We need to define getopt() and optind for them.
 */
extern int optind;
int getopt(int argc, char * const argv[], const char *optstring);

extern char* _PyOS_optarg;

#endif
