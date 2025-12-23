/*
 *                     OpenBIOS - free your system! 
 *                         ( FCode tokenizer )
 *                          
 *  stream.c - source program streaming from file.
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
#ifdef __GLIBC__
#define __USE_XOPEN_EXTENDED
#endif
#include <string.h>
#if __MWERKS__
#include <stat.h>
#include <extras.h>
#define strdup _strdup
#else
#include <sys/stat.h>
#endif

#include "toke.h"
#include "stream.h"

#define OUTPUT_SIZE	131072

extern bool offs16;
extern u16 nextfcode;

u8 *start, *pc, *end;
char *iname;

/* output pointers */
u8 *ostart, *opc, *oend;
char *oname;
static size_t ilen;

unsigned int lineno;

int init_stream( const char *name)
{
	FILE *infile;
	unsigned int i;
	
	struct stat finfo;
	
	if (stat(name,&finfo))
		return -1;
	
	ilen=finfo.st_size;
	start=malloc(ilen+1);
	if (!start)
		return -1;

	infile=fopen(name,"r");
	if (!infile)
		return -1;

	if (fread(start, ilen, 1, infile)!=1) {
		free(start);
		return -1;
	}

	fclose(infile);
	
	/* no zeroes within the file. */
	for (i=0; i<ilen; i++) {
		/* start[i]=start[i]?start[i]:0x0a; */
		start[i]=start[i]?start[i]:'\n';
		/*
		start[i] |= (((signed char)start[i] - 1) >> 7) & 0x0a;
		*/ /* this sucks because it messes up chars >= 0x80 */
		
/*		
	for (i=0; i < 255; i++) {
		printf( "%02lx %02lx %02lx\n", i, (long)((((signed char)i - 1) >> 7) & 0x0a), i | (long)((((signed char)i - 1) >> 7) & 0x0a) );
	}

		0	ffff	01	ff	0a
		1   0000	00		1
		2	0001	00		2
		80	ff7f	00	fe	8a
		81	ff80	01	ff	8b
		82	ff81	01	ff	8a
		ff	fffe	01	ff	ff
*/
	}
	start[ilen]=0;
	
	pc=start; 
	end=pc+ilen;

	iname=strdup(name);
	
	lineno=1;
	nextfcode=0x800;
	
	return 0;
	
}

int init_output( const char *in_name, const char *out_name )
{
	const char *ext;
	size_t len;

	/* preparing output */
	
	if( out_name )
		oname = strdup( out_name );
	else {
		ext=strrchr(in_name, '.');
		len=ext ? (ext-in_name) : strlen(in_name) ;
		oname=malloc(len+4);
		memcpy(oname, in_name, len);
		oname[len] = 0;
		strcat(oname, ".fc");
	}

	/* output buffer size. this is 128k per default now, but we
	 * could reallocate if we run out. KISS for now.
	 */
	ostart=malloc(OUTPUT_SIZE);
	if (!ostart) {
		free(oname);
		free(start);
		return -1;
	}

	opc=oend=ostart;
	oend+=OUTPUT_SIZE;
	
	return 0;
}

int close_stream(void)
{
	free(start);
	free(iname);
	return 0;	
}

int close_output(void)
{
	FILE *outfile;
	size_t len;

	len=(unsigned long)opc-(unsigned long)ostart;
	
	outfile=fopen(oname,"w");
	if (!outfile) {
		printf("toke: error opening output file.\n");
		return -1;
	}
	
	if(fwrite(ostart, len, 1, outfile)!=1)
		printf ("toke: error while writing output.\n");

	fclose(outfile);

	printf("toke: wrote %d bytes to bytecode file '%s'\n", (u32)len, oname);
	
	free(oname);
	return 0;
}

