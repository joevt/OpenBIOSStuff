/*
 *                     OpenBIOS - free your system! 
 *                            ( detokenizer )
 *                          
 *  detok.c parameter parsing and main detokenizer loop.  
 *  
 *  This program is part of a free implementation of the IEEE 1275-1994 
 *  Standard for Boot (Initialization Configuration) Firmware.
 *
 *  Copyright (C) 2001-2003 Stefan Reinauer, <stepan@openbios.org>
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
#define LONGOPT
#include <getopt.h>
#elifdef LONGOPT
#include <getopt.h>
#else
#include "getopt.h"
#endif

#ifdef __MWERKS__
	#include <getopt.h>
	#ifdef __IS_SIOUX_APP__
		#include <console.h>
	#endif
	#ifdef MPW_TOOL
		#include <CursorCtl.h>
	#endif
#endif

#include "detok.h"
#include "stream.h"
#include "decode.h"
#include "stack.h"

#define DETOK_VERSION "0.6"

/* prototypes for dictionary handling */
void init_dictionary(void);
/* prototype for detokenizer function */
int  detokenize(void);

static void print_copyright(void)
{
	static int printedcopyright = 0;
	if (!printedcopyright)
	{
		printf( "Welcome to the OpenBIOS detokenizer v%s\ndetok Copyright(c) 2001-2003 by Stefan Reinauer.\n"
		"Written by Stefan Reinauer, <stepan@openbios.org>\n"
		"This program is free software; you may redistribute it under the terms of\n"
		"the GNU General Public License.  This program has absolutely no warranty.\n"
		"Compiled " __TIME__ " " __DATE__ "\n\n" ,DETOK_VERSION);
		printedcopyright = 1;
	}
}

static void usage(char *name)
{
	static int printedusage = 0;
	if (!printedusage)
	{
		printf( "usage: %s [OPTION]... [FCODE-FILE]...\n\n"
			"         -v, --verbose          print fcode numbers\n"
			"         -h, --help             print this help text\n"
			"         -a, --all              don't stop at end0\n"
			"         -i, --ignorelen        do entire file, ingnoring length in start fcode\n"
			"         -n, --linenumbers      print line numbers\n"
			"         -o, --offsets          print byte offsets\n"
			"         -t, --tabs             use tabs for indenting instead of spaces\n"
			"         -m, --macrom [1,2,4,8,256] file is from a Macintosh ROM (1=Power Mac 8600, 2=Beige G3, 4=Power Mac G5, 8=B&W G3, 256=Development)\n"
			"         -s, --startoffset      start offset of rom dump\n"
			"         -d, --debugcapstone    output capstone info for macrom disassembly\n"
		, name);
		printedusage = 1;
	}
}


int main(int argc, char **argv)
{
	int c;
	const char *optstring="vhaim:s:notd?";

#ifdef MPW_TOOL
	InitCursorCtl(NULL);
	#define optarg _PyOS_optarg
#endif

#ifdef __IS_SIOUX_APP__
	argc = ccommand(&argv);
#endif

	while (1) {
#ifdef LONGOPT
		int option_index = 0;
		static struct option long_options[] = {
			{ "verbose", 0, 0, 'v' },
			{ "help", 0, 0, 'h' },
			{ "all", 0, 0, 'a' },
			{ "ignorelen", 0, 0, 'i' },
			{ "debugcapstone", 0, 0, 'd' },
			{ "macrom", 0, 0, 'm' },
			{ "startoffset", 0, 0, 's' },
			{ "linenumbers", 0, 0, 'n' },
			{ "offsets", 0, 0, 'o' },
			{ "tabs", 0, 0, 't' },
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
		case 'a':
			decode_all=1;
			break;
		case 'i':
			ignore_len=1;
			break;
		case 'm':
			if (optarg==NULL || optarg[0]==0)
				mac_rom = 0x20; // generic rom pre-G5 with named mac_rom_code_xxx tokens for:
				// (val) (i-val) b<to> b<to>1 (i-to) (var) (i-var) (defer) (i-defer) (field) b<lit> b<'> {'} b<">
			else
			{
				mac_rom=atoi( optarg );
				if (mac_rom < 1 || mac_rom > 0xfff)
					goto badoption;
			}
			break;
		case 's':
			if (optarg==NULL || optarg[0]==0)
				romstartoffset = 0;
			else
			{
				if ( !sscanf( optarg, "$%x", &romstartoffset ) )
				if ( !sscanf( optarg, "0x%x", &romstartoffset ) )
				if ( !sscanf( optarg, "%d", &romstartoffset ) )
					goto badoption;
			}
			break;
		case 'd':
			debugcapstone=1;
			break;
		case 'n':
			linenumbers|=1;
			break;
		case 'o':
			linenumbers|=2;
			break;
		case 't':
			use_tabs=1;
			break;
		case 'h':
		case '?':
			print_copyright();
			usage(argv[0]);
			return 0;		
		default:
badoption:
			print_copyright();
			printf ("%s: unknown option.\n",argv[0]);
			usage(argv[0]);
			return 1;
		}
	}

/*
	if (verbose)
		print_copyright();
*/

/*	
	if (linenumbers>2)
		printf("Line numbers will be disabled in favour of offsets.\n");
*/

	if (optind >= argc) {
		print_copyright();
		printf ("%s: filename missing.\n",argv[0]);
		usage(argv[0]);
		return 1;
	}

	init_dictionary();
	init_stack();

	while (optind < argc) {
		clear_stack();
		
		if (init_stream(argv[optind])) {
			printf ("Could not open file \"%s\".\n",argv[optind]);
			optind++;
			continue;
		}
		
		if ( filelen > 0x80000000 ) {
			printf ("File is too large \"%s\".\n",argv[optind]);
			optind++;
			continue;
		}
			
		if (filelen + romstartoffset > 0x100000000 ) {
			printf ("startoffset is too large \"%s\".\n",argv[optind]);
			optind++;
			continue;
		}
		
		fclen=(u32)filelen;
		detokenize();
		close_stream();
		
		optind++;
	}
	
	printf("\n");
	
	return 0;
}
