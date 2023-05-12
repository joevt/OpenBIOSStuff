/*
 *                     OpenBIOS - free your system! 
 *                         ( FCode tokenizer )
 *                          
 *  emit.c - fcode emitter.
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
#ifndef __MWERKS__
#if 0
	#include "mconfig.h"
#endif
#ifdef HAVE_ENDIAN_H
#include <endian.h>
#else
#include <machine/endian.h>
#define __BYTE_ORDER BYTE_ORDER
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#define __BIG_ENDIAN BIG_ENDIAN
#endif
#endif
#include "toke.h"
#include "stack.h"
#include "emit.h"

extern bool offs16;
extern int verbose;
extern u8 *ostart, *opc, *oend;

/* PCI data */
extern u16 pci_vpd, pci_revision;
extern bool pci_is_last_image;
extern u16 processor_architecture;
extern u16 pci_data_structure_start;
extern u16 pci_data_structure_length;
extern u32 rom_size;


/* header pointers */
extern u8  *fcode_hdr;
extern u8  *pci_hdr;

extern bool haveend;

u16 lookup_token(char *name);

int emit_byte(u8 data)
{
	u8 *newout;
	int newsize;

	if(opc==oend) {
		/* need more output space */
		newsize = (oend - ostart) * 2;
		printf("Increasing output buffer to %d bytes.\n", newsize);
		if ((newout=realloc(ostart, newsize)) == NULL) {
			printf("toke: could not allocate %d bytes for output buffer\n", newsize);
			exit(-1);
		}

		/* move pointers */
		opc=newout+(opc-ostart);
		ostart=newout;
		oend=ostart+newsize;
	}
	
	*opc=data;
	opc++;

	return 0;
}

int emit_fcode(u16 tok)
{
	if ((tok>>8))
		emit_byte(tok>>8);

	emit_byte(tok);
	
	return 0;
}

int emit_token(const char *name)
{
	return emit_fcode(lookup_token((char *)name));
}
	
int emit_num32(u32 num)
{
	emit_byte(num>>24);
	emit_byte(num>>16);
	emit_byte(num>>8);
	emit_byte(num);

	return 0;
}

int emit_num16(u16 num)
{
	emit_byte(num>>8);
	emit_byte(num);

	return 0;
}

int emit_offset(long offs)
{
	if (offs16)
	{
		if (offs < -32768 || offs > 32767)
			printf("warning: 16 bit offset is out of range\n");
		emit_num16(offs);
	}
	else
	{
		if (offs < -128 || offs > 127)
			printf("warning: 8 bit offset is out of range\n");
		emit_byte(offs);
	}
	
	return 0;
}

int emit_num(u32 num)
{
	int result;
	switch (num)
	{
		case 0:
			return emit_token("0");
		case 1:
			return emit_token("1");
		case 2:
			return emit_token("2");
		case 3:
			return emit_token("3");
		case (u32)(-1):
			return emit_token("-1");
		default:
			result = emit_token("b(lit)");
			if (result == 0)
				result = emit_num32(num);
			return result;
	}
}


s16 receive_offset(void)
{
	s16 offs=0;
	
	if (offs16) {
		offs=((*opc)<<8)|(*(opc+1));
	} else
		offs=(*opc);

	return offs;
}

int emit_string(u8 *string, unsigned int cnt)
{
	unsigned int i=0;
	
	if (cnt>255) {
		printf("string too long.");
		exit(1);
	}
	emit_byte(cnt);
	for (i=0; i<cnt; i++)
		emit_byte(string[i]);

	return 0;
}

int emit_fcodehdr(void)
{
	/* We comply with IEEE 1275-1994 */
	emit_byte(0x08);
	
	/*  checksum */
	emit_num16(0);

	/* len */
	emit_num32(0);

	return 0;
}

int finish_fcodehdr(void)
{
	u16 checksum=0;
	u32 len,i;

	if(!fcode_hdr)
	{
		printf("warning: trying to fix up unknown fcode header\n");
		return -1;
	}

	/* If the program does not do this, we do */
	if (!haveend)
		emit_token("end0");

	len=(unsigned long)opc-(unsigned long)(fcode_hdr-1);
	
	for (i=8;i<len;i++)
		checksum+=fcode_hdr[i-1];

	dpushtype(kFCodeHeader, (unsigned long)opc);
	opc=fcode_hdr+1;
	emit_num16(checksum);
	emit_num32(len);
	opc=(u8 *)dpoptype(kFCodeHeader);
	if (verbose)
		printf("toke: checksum is 0x%04x (%d bytes)\n", 
							checksum, len);

	fcode_hdr=NULL;
	haveend=FALSE;
	return 0;
}

int emit_pcihdr(u16 vid, u16 did, u32 classid)
{
	int i;
	
	/* PCI start signature */
	emit_byte(0x55); emit_byte(0xaa);
	
	/* Processor architecture */
	emit_byte(0x00); emit_byte(0x00);

	/* 20 bytes of padding */
	for (i=0; i<20; i++) emit_byte(0x00);

	/* pointer to start of PCI data structure */
	emit_byte(pci_data_structure_start); emit_byte(pci_data_structure_start>>8);

	/* 2 bytes of zero padding */
	emit_byte(0x00); emit_byte(0x00);

	for (i=0; i<pci_data_structure_start-0x1c; i++) emit_byte(0x00);
	
	/* PCI Data structure */
	emit_byte('P'); emit_byte('C'); emit_byte('I'); emit_byte('R');
	/* vendor id */
	emit_byte(vid); emit_byte(vid>>8);
	/* device id */
	emit_byte(did); emit_byte(did>>8);
	/* vital product data */
	emit_byte(0x00); emit_byte(0x00);
	/* length of pci data structure */
	emit_byte(pci_data_structure_length); emit_byte(pci_data_structure_length>>8);
	/* PCI data structure revision */
	emit_byte(0x00);
	/* reg level programming or programming interface code */
	emit_byte(classid);
	/* subclass code */
	emit_byte(classid>>8);
	/* class code */
	emit_byte(classid>>16);
	/* size of image - to be filled later */
	emit_byte(0x00); emit_byte(0x00);
	/* revision level */
	emit_byte(0x00); emit_byte(0x00);
	/* code type = open firmware */
	emit_byte(0x01);
	emit_byte(0x80);
	/* 2 bytes of padding */
	emit_byte(0x00); emit_byte(0x00);
	
	for (i=0; i<pci_data_structure_length-0x18; i++) emit_byte(0x00);

	return 0;
}


int finish_pcihdr(void)
{
	u8 *tpc;
	u32 imgsize=opc-ostart, imgblocks;
	int padding;
	
	if(!pci_hdr)
	{
		printf("error: trying to fix up unknown pci header\n");
		return -1;
	}

	tpc=opc;

	/* fix up processor architecture */
	opc=pci_hdr+2;
	emit_byte(processor_architecture); emit_byte(processor_architecture>>8);

	/* fix up vpd */
	opc=pci_hdr+pci_data_structure_start+8;
	emit_byte(pci_vpd); emit_byte(pci_vpd>>8);

	/* fix up image size */
	opc=pci_hdr+pci_data_structure_start+16;
	imgblocks=(imgsize+511)>>9; /* size is in 512byte blocks */
	emit_byte(imgblocks); emit_byte(imgblocks>>8);
	
	/* fix up revision */
	emit_byte(pci_revision); emit_byte(pci_revision>>8);
	
	/* fix up last image flag */
	opc++;
	emit_byte(pci_is_last_image?0x80:0x00);
	opc=tpc;
	
	/* align to 512bytes */
	padding=((imgsize+511)&-512)-imgsize;
	printf("Adding %d bytes of zero padding to PCI image.\n",padding);
	while (padding--)
		emit_byte(0);
	
	pci_hdr=NULL;
	return 0;
}

void finish_headers(void)
{
	if (fcode_hdr) finish_fcodehdr();
	if (pci_hdr) finish_pcihdr();
	if (rom_size) {
		int padding = rom_size-(opc-ostart);
		if (padding<0)
			printf("warning: rom size has been exceeded\n");
		else
		{
			printf("Adding %d bytes of zero padding to ROM image.\n", padding);
			while (padding--)
				emit_byte(0);
		}
	}
}

