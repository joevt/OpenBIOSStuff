/*
 *                     OpenBIOS - free your system! 
 *                         ( FCode tokenizer )
 *                          
 *  toke.c - main tokenizer loop and parameter parsing.
 *  
 *  This program is part of a free implementation of the IEEE 1275-1994 
 *  Standard for Boot (Initialization Configuration) Firmware.
 *
 *  Copyright (C) 2001-2004 Stefan Reinauer, <stepan@openbios.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __GLIBC__
	#define _GNU_SOURCE
	#include <getopt.h>
#endif

#ifdef __MWERKS__
	#include <getopt.h>
	#ifdef __IS_SIOUX_APP__
		#include <console.h>
	#endif
#endif

#include "toke.h"
#include "stream.h"
#include "stack.h"
#include "emit.h"

#define TOKE_VERSION "0.6.5"

int verbose=0;
int noerrors=0;
int lowercase=0;

int gGotError=0;

static void print_copyright(void)
{
	static int printedcopyright = 0;
	if (!printedcopyright)
	{
		printf( "Welcome to toke - the OpenBIOS tokenizer v%s\nCopyright (c)"
		" 2001-2004 by Stefan Reinauer, <stepan@openbios.org>\n"
		"This program is free software; you may redistribute it under the terms of\n"
		"the GNU General Public License. This program has absolutely no warranty.\n"
		"Compiled " __TIME__ " " __DATE__ "\n\n", TOKE_VERSION);
		printedcopyright = 1;
	}
}

static void usage(char *name)
{
	static int printedusage = 0;
	if (!printedusage)
	{
		printf("usage: %s [OPTION]... [-o target] <forth-file>\n\n"
			"         -v, --verbose          print fcode numbers\n"
			"         -a, --ignore-errors    ignore errors\n"
			"         -l, --lowercase        convert all names to lower case\n"
			"         -h, --help             print this help text\n\n", name);
		printedusage = 1;
	}
}

int main(int argc, char **argv)
{
	const char *optstring="vhilo:?";
	char *outputname = NULL;
	int c;

#ifdef MPW_TOOL
	/*InitCursorCtl(NULL);*/
	#define optarg _PyOS_optarg
#endif

#ifdef __IS_SIOUX_APP__
	argc = ccommand(&argv);
#endif
	
	while (1) {
#ifdef __GLIBC__
		int option_index = 0;
		static struct option long_options[] = {
			{ "verbose", 0, 0, 'v' },
			{ "ignore-errors", 0, 0, 'i' },
			{ "help", 0, 0, 'h' },
			{ "lowercase", 0, 0, 'l' },
			{ 0, 0, 0, 0 }
		};

		c = getopt_long (argc, argv, optstring,
			long_options, &option_index);
#else
		c = getopt (argc, argv, optstring);
#endif
		if (c == -1)
			break;

		switch (c) {
			case 'v':
				verbose=1;
				break;
			case 'o':
				outputname = optarg;
				break;
			case 'i':
				noerrors=1;
				break;
			case 'l':
				lowercase=1;
				break;
			case 'h':
			case '?':
				print_copyright();
				usage(argv[0]);
				return 0;		
			default:
				print_copyright();
				printf ("%s: unknown options.\n",argv[0]);
				usage(argv[0]);
				return 1;
		}
	}

	if (verbose)
		print_copyright();

	if (optind >= argc) {
		print_copyright();
		printf ("%s: filename missing.\n",argv[0]);
		usage(argv[0]);
		return 1;
	}

	init_stack();
	init_dictionary();
	init_macros();
	init_scanner();
	
	while (optind < argc) {
		char* inFileName;
		inFileName = argv[optind];
		
		if (init_stream(inFileName))
		{
			printf ("%s: warning: could not open file \"%s\"\n",
					argv[0], inFileName);
		}
		else
		{
			init_output(inFileName, outputname);
			
			tokenize();
			finish_headers();
			
			close_output();
			close_stream();
		}
		
		optind++;
	}
	
	exit_scanner();
	
	if (gGotError)
		exit(-1);
	return 0;
}

