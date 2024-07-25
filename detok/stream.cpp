/*
 *                     OpenBIOS - free your system! 
 *                            ( detokenizer )
 *                          
 *  stream.c - FCode program bytecode streaming from file.
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

#ifdef __GLIBC__
#include <stat.h>
#endif
#include <errno.h>

#if __MWERKS__
#include <stat.h>
#else
#include <sys/stat.h>
#endif

#include "detok.h"
#include "decode.h"
#include "stream.h"

u16 fcode;
u8  inbyte;

static u8 *indata, *pc, *max;

int init_stream(char *name)
{
	FILE *infile;
	struct stat finfo;
	
	if (stat(name,&finfo))
		return -1;

	filelen=finfo.st_size;

	indata=(u8*)malloc(finfo.st_size);
	if (!indata)
		return -1;

	infile=fopen(name,"r");
	if (!infile)
		return -1;

	if (fread(indata, finfo.st_size, 1, infile)!=1) {
		free(indata);
		return -1;
	}
	
	fclose(infile);

	pc=indata; 
	max=pc+finfo.st_size;
	
	return 0;
}

int close_stream(void)
{
	free(indata);
	return 0;	
}

u32 get_streampos(void)
{
	return (u32)(pc-indata);
}

void set_streampos(u32 pos)
{
	pc=indata+pos;
}

static int get_byte(void)
{
	if (pc>=max) {
		if ( gPass )
			printf ("\n\\ Unexpected end of file.\n");
		inbyte=0;
		return 0;
	}

	inbyte=*pc;
	pc++;

	return 1;
}

u16 get_token(void)
{
	u16 tok;
	get_byte();
	tok=inbyte;
	if (tok != 0x00 && tok < 0x10) {
		get_byte();
		tok<<=8;
		tok|=inbyte;
	}
	else if (mac_rom & 4 && tok == 0xf4) {
		tok = get_num16();
	}
	fcode=tok;
	return tok;
}

void get_mem(u8* dst, int size)
{
	while (size > 0) {
		get_byte();
		*dst++ = inbyte;
		size--;
	}
}

u32 get_num32(void)
{
	u32 ret;

	get_byte();
	ret=inbyte<<24;
	get_byte();
	ret|=(inbyte<<16);
	get_byte();
	ret|=(inbyte<<8);
	get_byte();
	ret|=inbyte;

	return ret;
}

u16 get_num16(void)
{
	u16 ret;

	get_byte();
	ret=inbyte<<8;
	get_byte();
	ret|=inbyte;

	return ret;
}

u8 get_num8(void)
{
	get_byte();
	return(inbyte);
}

s32 get_offset(bool islong)
{
	if (islong)
		return ((u16)get_num16());
	else if (offs16)
		return ((s16)get_num16());
	else
		return ((s8)get_num8());
}


int scnt=0;
char *get_string(u8 * len)
{
	u8 *data;
	u8 size;
	unsigned int i;

	get_byte();
	size=inbyte;
	if (len)
		*len = size;

	scnt++;
	
#ifdef FORTH_STRINGS
	data=malloc(size+2);

	if (!data)
	{
		printf ("No more memory.\n");
		exit(-ENOMEM);
	}
	data[0]=size;

	for (i=1; i<=size; i++) {
		get_byte();
		data[i]=inbyte;
	}
#else
	data=(u8 *)malloc(size+1);

	if (!data)
	{
		printf ("No more memory.\n");
		exit(-ENOMEM);
	}
	
	for (i=0; i<size; i++) {
		get_byte();
		data[i]=inbyte;
	}
#endif
	data[i]=0;
	return (char *)data;
}


char *get_string2(u8 * len, bool* isGoodString)
{
	u8 *data;
	u8 size;
	unsigned int i;

	get_byte();
	size=inbyte;

	if (pc + size > max)
	{
		size = max - pc;
		*isGoodString = FALSE;
	}
	else
		*isGoodString = TRUE;

	if (len)
		*len = size;

	scnt++;
	
	data=(u8 *)malloc(size+1);

	if (!data)
	{
		printf ("No more memory.\n");
		exit(-ENOMEM);
	}
	else
	{
		for (i=0; i<size; i++) {
			get_byte();
			data[i]=inbyte;
		}

		data[i]=0;
	}
	return (char *)data;
}
