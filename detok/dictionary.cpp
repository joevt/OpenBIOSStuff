/*
 *                     OpenBIOS - free your system!
 *                           ( detokenizer )
 *
 *  dictionary.c - dictionary initialization and functions.
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
#include "dictionary.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "decode.h"

int dictionaryIntialized = FALSE;

const char *fcerror="ferror";
const char *unnamed="(unnamed-fcode)";


token_t *dictionary=NULL;

static bool is_historical_fcode( u16 number )
{
	switch ( number )
	{
/*
IEEE Std 1275-1994

5.3.1.1.1 Historical FCode numbers

Historical FCode numbers correspond to FCode functions that are not defined by this standard, but that are or have
been used by FCode evaluators that predate this standard. These numbers are reserved for the benefit of those pre-existing
systems and are not available for reassignment by future revisions of this standard. The historical FCode
numbers are interspersed within the range 0x000 through 0x2FF.

Historical FCode numbers include the other previously implemented FCodes below and the non-implemented FCodes further below except "intr".

H.2 Obsolete FCodes

H.2.1 Previously implemented FCodes

Pre-Open Firmware versions of SBus [B2]firmware have used the following FCodes. None of these are required for
a Open Firmware implementation; however, some implementations may choose to support these for the purpose of
backwards compatibility.

H.2.1.1 Generic 1-bit frame-buffer support

The Òfb1Ó generic frame-buffer support package implements the display device low-level interfaces for frame-buffers
with one memory bit per pixel. It applies only to frame buffers organized as a series of doublets with big-endian
addressing, with the most significant bit within a doublet corresponding to the leftmost pixel within the
group of sixteen pixels controlled by that doublet. In normal (not inverse) video mode, background pixels are
drawn with zero-bits, and foreground pixels with one-bits.
*/
					case 0x170: /* fb1-draw-character */
					case 0x171: /* fb1-reset-screen */
					case 0x172: /* fb1-toggle-cursor */
					case 0x173: /* fb1-erase-screen */
					case 0x174: /* fb1-blink-screen */
					case 0x175: /* fb1-invert-screen */
					case 0x176: /* fb1-insert-characters */
					case 0x177: /* fb1-delete-characters */
					case 0x178: /* fb1-insert-lines */
					case 0x179: /* fb1-delete-lines */
					case 0x17A: /* fb1-draw-logo */
					case 0x17B: /* fb1-install */
					case 0x17C: /* fb1-slide-up */
/*
H.2.1.2 Other previously implemented FCodes
*/
					case 0x101: /* dma-alloc */
					case 0x104: /* memmap */
					case 0x106: /* >physical */
					case 0x10F: /* my-params */
					case 0x117: /* intr */
					case 0x118: /* driver */
					case 0x123: /* group-code */
					case 0x210: /* processor-type */
					case 0x211: /* firmware-version */
					case 0x212: /* fcode-version */
					case 0x238: /* probe */
					case 0x239: /* probe-virtual */
/*			
H.2.2 Non-implemented FCodes

Pre-Open Firmware systems assigned the following FCode numbers, but the functions were not supported. To
avoid any possible confusion, however, these FCode numbers are reserved and should not be reassigned.
*/
					case 0x229: /* adr-mask */
					case 0x0BF: /* b(code) */
					case 0x0FE: /* 4-byte-id */
					case 0x0A1: /* convert */
					case 0x156: /* frame-buffer-busy? */
					case 0x1A2: /* poll-packet */
					case 0x1A0: /* return-buffer */
					case 0x0B3: /* set-token-table */
					case 0x0B4: /* set-table */
					case 0x190: /* VME support words */
					case 0x191:
					case 0x192:
					case 0x193:
					case 0x194:
					case 0x195:
					case 0x196:
					case 0x1A1: /* xmit-packet */
						return TRUE;
/*
Writing FCode 3.x Programs

TABLE D-3 FCode 2.x Commands Deleted in FCode 3.x

The historical FCodes "dma-alloc" to "probe-virtual", and the non-implemented FCode "4-byte-id" that existed
in FCode 2.x do not exist in FCode 3.x
*/
					default:
						return FALSE;
	}
}


static bool is_new_fcode( u16 number )
{
	switch ( number )
	{
/*
IEEE Std 1275-1994

H.4 New FCodes and methods

Most pre-Open Firmware systems do not implement the following FCodes and methods:
*/
					case 0x089: /* unloop */
					case 0x0da: /* get-token */
					case 0x0db: /* set-token */
					case 0x0dc: /* state */
					case 0x0dd: /* compile, */
					case 0x0de: /* behavior */
					case 0x128: /* decode-phys */
					case 0x226: /* lwflip */
					case 0x227: /* lbflip */
					case 0x228: /* lbflips */
					case 0x23d: /* next-property */
					case 0x23e: /* byte-load */
					case 0x23f: /* set-args */
					/*
					TABLE D-5 Differently Functioning 3.x FCodes With Changed Byte Values
					*/
					case 0xC7: /* #		Not the same as old # (now called u#). */
					case 0xC8: /* #s	Not the same as old #s (now called u#s). */
					case 0xC9: /* #>	Not the same as old #> (now called u#>). */
/*
Writing FCode 3.x Programs

TABLE D-4 New FCodes Added in 3.x

Also includes FCodes "unloop" to "set-args".
*/
					case 0x129: /* push-package */
					case 0x12A: /* pop-package */
					case 0x12B: /* interpose */
						return TRUE;
					default:
						return FALSE;
	}
}

token_t *find_token(u16 number)
{
	token_t *curr;
	
	for (curr=dictionary; curr!=NULL; curr=curr->next)
		if (curr->fcode==number)
		if ( ( curr->flags & kIgnoreToken ) == 0 )
			return curr;

	return NULL;
}

const char *get_token_name(token_t *theToken)
{
	if (theToken)
		return theToken->name;

	return fcerror;
}

const char *lookup_token(u16 number)
{
	token_t *curr = find_token( number );
	return get_token_name( curr );
}

token_t *add_token(u16 number, const char *name)
{
/* get token and name */
	token_t *curr = find_token(number);
	const char* oldname = get_token_name(curr);

	const char* token_type_string;
	char token_flags_string[100];

/* get token flags */
	u16 new_flags;
	bool make_new_token = FALSE;

	if ( curr )
	{
		new_flags = curr->flags;
	}
	else
	{
		if ( is_historical_fcode( number ) )
			new_flags = kFCodeHistorical;
		else if ( is_new_fcode( number ) )
			new_flags = kFCodeNew;
		else if ( number < 0x600 )
			new_flags = kFCodeReserved;
		else if ( number < 0x800 )
			new_flags = kFCodeVendorUnique;
		else if ( number < 0x1000 )
			new_flags = kFCodeProgramDefined;
		else if ( number < 0x8000 )
			new_flags = kFCodeExtendedRange;
		else
			new_flags = kFCodeNegativeRange;
	}

	if ( dictionaryIntialized )
		new_flags |= kFCodeDefined;
	else
		new_flags |= kFCodePredefined;

/* get token flags string */
	token_flags_string[0] = 0;
	if ( new_flags & kFCodePredefined )
		strcat( token_flags_string, "predefined " );


/* get token type string */
	switch ( new_flags & kFCodeTypeMask )
	{
		case kFCodeHistorical:
			token_type_string = "historical ";
			break;
		case kFCodeNew:
			token_type_string = "new ";
			break;
		case kFCodeReserved:
			token_type_string = "reserved ";
			break;
		case kFCodeVendorUnique:
			token_type_string = "vendor unique ";
			break;
		case kFCodeProgramDefined:
			token_type_string = "program defined ";
			break;
		case kFCodeExtendedRange:
			token_type_string = "extended range ";
			break;
		case kFCodeNegativeRange:
			token_type_string = "negative range ";
			break;
		default:
			token_type_string = "** UNKNOWN ** ";
			break;
	}
	

/* output token message */

	if ( curr == NULL )
	{
		if ( dictionaryIntialized )
			fprintf(stderr, "Line %d # Defined %s%stoken 0x%03x \"%s\"\n", linenum, token_flags_string, token_type_string, number, name);
		make_new_token = TRUE;
	}
	else
	{
		const char* error_string = "Unknown definition occurance for "; /* used if something weird happens */
		const char* message_string = error_string;
/*
		old						new						message
		defined predefined		defined predefined		
		
		0		1				1		1				Defined predefined

		1		0				1		0				Redefined
		1		1				1		1				Redefined predefined
*/
		
		
		switch ( curr->flags & (kFCodeDefined | kFCodePredefined) ) {
			case kFCodePredefined:
				switch ( new_flags & (kFCodeDefined | kFCodePredefined) ) {
					case kFCodeDefined | kFCodePredefined	:	message_string = "Defined "; break;
				} break;
			case kFCodeDefined:
				switch ( new_flags & (kFCodeDefined | kFCodePredefined) ) {
					case kFCodeDefined						:	message_string = "Redefined "; break;
				} break;
			case kFCodeDefined | kFCodePredefined:
				switch ( new_flags & (kFCodeDefined | kFCodePredefined) ) {
					case kFCodeDefined | kFCodePredefined	:	message_string = "Redefined "; break;
				} break;
		}

		if ( name == NULL )
		{
			fprintf(stderr, "Line %d # Ignored attempt to rename token 0x%03x \"%s\"\n", linenum, number, oldname);
			name = oldname;
			make_new_token = (curr->flags & kFCodeDefined) != 0;
		}
		else if ( strcmp( oldname, name ) != 0 )
		{
			if ( name == unnamed && number < 0x600 )
			{
				/* reserved tokens should not be no-names so report warning */
				fprintf(stderr, "Line %d # Ignored attempt to make %s%stoken 0x%03x \"%s\" unnamed\n", linenum, token_flags_string, token_type_string, number, oldname);
				name = oldname;
				make_new_token = (curr->flags & kFCodeDefined) != 0;
			}
			else
			{
				fprintf(stderr, "Line %d # Renamed %s%stoken 0x%03x from \"%s\" to \"%s\"\n", linenum, token_flags_string, token_type_string, number, oldname, name);
				make_new_token = TRUE;
			}
		}
		else
		{
			fprintf(stderr, "Line %d # %s%s%stoken 0x%03x \"%s\"\n", linenum, message_string, token_flags_string, token_type_string, number, name);
			message_string = NULL;
			make_new_token = (curr->flags & kFCodeDefined) != 0;
		}
		
		if ( message_string == error_string )
			fprintf(stderr, "Line %d # %s%s%stoken 0x%03x \"%s\"\n", linenum, message_string, token_flags_string, token_type_string, number, name);
	}
	
/* make new token */
	if ( make_new_token )
	{
		curr=(token_t*)malloc(sizeof(token_t));
		if(!curr) {
			printf("Out of memory while adding token.\n");
			exit(-ENOMEM);
		}

		curr->next = dictionary;
		curr->fcode = number;
		curr->name = name;
		curr->ofname = NULL;
		curr->execution_pos = -1;
		curr->hlink_pos = -1;
		dictionary = curr;
	}

	curr->flags = new_flags;

	return curr;
}

static void make_of_token( token_t *token, const char * ofname ) {
	token->ofname = ofname;
}

#if 0
void clear_program_tokens(void)
{
	token_t *curr;
	token_t *last = NULL;
	token_t *next;
	
	for (curr=dictionary; curr!=NULL; curr=next)
	{
		next = curr->next;
		if (curr->fcode >= 0x800)
#if 0
		{
			if ( last == NULL )
				dictionary = next;
			else
				last->next = next;
			free( curr ); /* donÕt free token name because it might be a string constant */
		}
		else
			last = curr;
#else
			curr->flags |= kIgnoreToken;
#endif
	}
}
#endif

void init_dictionary(void)
{
	add_token( 0x000, "end0" );
	add_token( 0x010, "b(lit)" );
	add_token( 0x011, "b(')" );
	add_token( 0x012, "b(\")" );
	add_token( 0x013, "bbranch" );
	add_token( 0x014, "b?branch" );
	add_token( 0x015, "b(loop)" );
	add_token( 0x016, "b(+loop)" );
	add_token( 0x017, "b(do)" );
	add_token( 0x018, "b(?do)" );
	add_token( 0x019, "i" );
	add_token( 0x01a, "j" );
	add_token( 0x01b, "b(leave)" );
	add_token( 0x01c, "b(of)" );
	add_token( 0x01d, "execute" );
	add_token( 0x01e, "+" );
	add_token( 0x01f, "-" );
	add_token( 0x020, "*" );
	add_token( 0x021, "/" );
	add_token( 0x022, "mod" );
	add_token( 0x023, "and" );
	add_token( 0x024, "or" );
	add_token( 0x025, "xor" );
	add_token( 0x026, "invert" );
	add_token( 0x027, "lshift" );
	add_token( 0x028, "rshift" );
	add_token( 0x029, ">>a" );
	add_token( 0x02a, "/mod" );
	add_token( 0x02b, "u/mod" );
	add_token( 0x02c, "negate" );
	add_token( 0x02d, "abs" );
	add_token( 0x02e, "min" );
	add_token( 0x02f, "max" );
	add_token( 0x030, ">r" );
	add_token( 0x031, "r>" );
	add_token( 0x032, "r@" );
	add_token( 0x033, "exit" );
	add_token( 0x034, "0=" );
	add_token( 0x035, "0<>" );
	add_token( 0x036, "0<" );
	add_token( 0x037, "0<=" );
	add_token( 0x038, "0>" );
	add_token( 0x039, "0>=" );
	add_token( 0x03a, "<" );
	add_token( 0x03b, ">" );
	add_token( 0x03c, "=" );
	add_token( 0x03d, "<>" );
	add_token( 0x03e, "u>" );
	add_token( 0x03f, "u<=" );
	add_token( 0x040, "u<" );
	add_token( 0x041, "u>=" );
	add_token( 0x042, ">=" );
	add_token( 0x043, "<=" );
	add_token( 0x044, "between" );
	add_token( 0x045, "within" );
	add_token( 0x046, "drop" );
	add_token( 0x047, "dup" );
	add_token( 0x048, "over" );
	add_token( 0x049, "swap" );
	add_token( 0x04A, "rot" );
	add_token( 0x04b, "-rot" );
	add_token( 0x04c, "tuck" );
	add_token( 0x04d, "nip" );
	add_token( 0x04e, "pick" );
	add_token( 0x04f, "roll" );
	add_token( 0x050, "?dup" );
	add_token( 0x051, "depth" );
	add_token( 0x052, "2drop" );
	add_token( 0x053, "2dup" );
	add_token( 0x054, "2over" );
	add_token( 0x055, "2swap" );
	add_token( 0x056, "2rot" );
	add_token( 0x057, "2/" );
	add_token( 0x058, "u2/" );
	add_token( 0x059, "2*" );
	add_token( 0x05a, "/c" );
	add_token( 0x05b, "/w" );
	add_token( 0x05c, "/l" );
	add_token( 0x05d, "/n" );
	add_token( 0x05e, "ca+" );
	add_token( 0x05f, "wa+" );
	add_token( 0x060, "la+" );
	add_token( 0x061, "na+" );
	add_token( 0x062, "char+" );
	add_token( 0x063, "wa1+" );
	add_token( 0x064, "la1+" );
	add_token( 0x065, "cell+" );
	add_token( 0x066, "chars" );
	add_token( 0x067, "/w*" );
	add_token( 0x068, "/l*" );
	add_token( 0x069, "cells" );
	add_token( 0x06a, "on" );
	add_token( 0x06b, "off" );
	add_token( 0x06c, "+!" );
	add_token( 0x06d, "@" );
	add_token( 0x06e, "l@" );
	add_token( 0x06f, "w@" );
	add_token( 0x070, "<w@" );
	add_token( 0x071, "c@" );
	add_token( 0x072, "!" );
	add_token( 0x073, "l!" );
	add_token( 0x074, "w!" );
	add_token( 0x075, "c!" );
	add_token( 0x076, "2@" );
	add_token( 0x077, "2!" );
	add_token( 0x078, "move" );
	add_token( 0x079, "fill" );
	add_token( 0x07a, "comp" );
	add_token( 0x07b, "noop" );
	add_token( 0x07c, "lwsplit" );
	add_token( 0x07d, "wljoin" );
	add_token( 0x07e, "lbsplit" );
	add_token( 0x07f, "bljoin" );
	add_token( 0x080, "wbflip" );
	add_token( 0x081, "upc" );
	add_token( 0x082, "lcc" );
	add_token( 0x083, "pack" );
	add_token( 0x084, "count" );
	add_token( 0x085, "body>" );
	add_token( 0x086, ">body" );
	add_token( 0x087, "fcode-revision" );
	add_token( 0x088, "span" );
	add_token( 0x08a, "expect" );
	add_token( 0x08b, "alloc-mem" );
	add_token( 0x08c, "free-mem" );
	add_token( 0x08d, "key?" );
	add_token( 0x08e, "key" );
	add_token( 0x08f, "emit" );
	add_token( 0x090, "type" );
	add_token( 0x091, "(cr" );
	add_token( 0x092, "cr" );
	add_token( 0x093, "#out" );
	add_token( 0x094, "#line" );
	add_token( 0x095, "hold" );
	add_token( 0x096, "<#" );
	add_token( 0x097, "u#>" );
	add_token( 0x098, "sign" );
	add_token( 0x099, "u#" );
	add_token( 0x09a, "u#s" );
	add_token( 0x09b, "u." );
	add_token( 0x09c, "u.r" );
	add_token( 0x09d, "." );
	add_token( 0x09e, ".r" );
	add_token( 0x09f, ".s" );
	add_token( 0x0a0, "base" );
	add_token( 0x0a1, "convert" );
	add_token( 0x0a2, "$number" );
	add_token( 0x0a3, "digit" );
	add_token( 0x0a4, "-1" );
	add_token( 0x0a5, "0" );
	add_token( 0x0a6, "1" );
	add_token( 0x0a7, "2" );
	add_token( 0x0a8, "3" );
	add_token( 0x0a9, "bl" );
	add_token( 0x0aa, "bs" );
	add_token( 0x0ab, "bell" );
	add_token( 0x0ac, "bounds" );
	add_token( 0x0ad, "here" );
	add_token( 0x0ae, "aligned" );
	add_token( 0x0af, "wbsplit" );
	add_token( 0x0b0, "bwjoin" );
	add_token( 0x0b1, "b(<mark)" );
	add_token( 0x0b2, "b(>resolve)" );
	add_token( 0x0b3, "set-token-table" );
	add_token( 0x0b4, "set-table" );
	add_token( 0x0b5, "new-token" );
	add_token( 0x0b6, "named-token" );
	add_token( 0x0b7, "b(:)" );
	add_token( 0x0b8, "b(value)" );
	add_token( 0x0b9, "b(variable)" );
	add_token( 0x0ba, "b(constant)" );
	add_token( 0x0bb, "b(create)" );
	add_token( 0x0bc, "b(defer)" );
	add_token( 0x0bd, "b(buffer:)" );
	add_token( 0x0be, "b(field)" );
	add_token( 0x0bf, "b(code)" );
	add_token( 0x0c0, "instance" );
	add_token( 0x0c2, "b(;)" );
	add_token( 0x0c3, "b(to)" );
	add_token( 0x0c4, "b(case)" );
	add_token( 0x0c5, "b(endcase)" );
	add_token( 0x0c6, "b(endof)" );
	add_token( 0x0c7, "#" );
	add_token( 0x0c8, "#s" );
	add_token( 0x0c9, "#>" );
	add_token( 0x0ca, "external-token" );
	add_token( 0x0cb, "$find" );
	add_token( 0x0cc, "offset16" );
	add_token( 0x0cd, "evaluate" );
	add_token( 0x0d0, "c," );
	add_token( 0x0d1, "w," );
	add_token( 0x0d2, "l," );
	add_token( 0x0d3, "," );
	add_token( 0x0d4, "um*" );
	add_token( 0x0d5, "um/mod" );
	add_token( 0x0d8, "d+" );
	add_token( 0x0d9, "d-" );
	add_token( 0x0f0, "start0" );
	add_token( 0x0f1, "start1" );
	add_token( 0x0f2, "start2" );
	add_token( 0x0f3, "start4" );
	add_token( 0x0fc, "ferror" );
	add_token( 0x0fd, "version1" );
	add_token( 0x0fe, "4-byte-id" );
	add_token( 0x0ff, "end1" );
	add_token( 0x101, "dma-alloc" );
	add_token( 0x102, "my-address" );
	add_token( 0x103, "my-space" );
	add_token( 0x104, "memmap" );
	add_token( 0x105, "free-virtual" );
	add_token( 0x106, ">physical" );
	add_token( 0x10f, "my-params" );
	add_token( 0x110, "property" );
	add_token( 0x111, "encode-int" );
	add_token( 0x112, "encode+" );
	add_token( 0x113, "encode-phys" );
	add_token( 0x114, "encode-string" );
	add_token( 0x115, "encode-bytes" );
	add_token( 0x116, "reg" );
	add_token( 0x117, "intr" );
	add_token( 0x118, "driver" );
	add_token( 0x119, "model" );
	add_token( 0x11a, "device-type" );
	add_token( 0x11b, "parse-2int" );
	add_token( 0x11c, "is-install" );
	add_token( 0x11d, "is-remove" );
	add_token( 0x11e, "is-selftest" );
	add_token( 0x11f, "new-device" );
	add_token( 0x120, "diagnostic-mode?" );
	add_token( 0x121, "display-status" );
	add_token( 0x122, "memory-test-suite" );
	add_token( 0x123, "group-code" );
	add_token( 0x124, "mask" );
	add_token( 0x125, "get-msecs" );
	add_token( 0x126, "ms" );
	add_token( 0x127, "finish-device" );
	add_token( 0x130, "map-low" );
	add_token( 0x131, "sbus-intr>cpu" );
	add_token( 0x150, "#lines" );
	add_token( 0x151, "#columns" );
	add_token( 0x152, "line#" );
	add_token( 0x153, "column#" );
	add_token( 0x154, "inverse?" );
	add_token( 0x155, "inverse-screen?" );
	add_token( 0x156, "frame-buffer-busy?" );
	add_token( 0x157, "draw-character" );
	add_token( 0x158, "reset-screen" );
	add_token( 0x159, "toggle-cursor" );
	add_token( 0x15a, "erase-screen" );
	add_token( 0x15b, "blink-screen" );
	add_token( 0x15c, "invert-screen" );
	add_token( 0x15d, "insert-characters" );
	add_token( 0x15e, "delete-characters" );
	add_token( 0x15f, "insert-lines" );
	add_token( 0x160, "delete-lines" );
	add_token( 0x161, "draw-logo" );
	add_token( 0x162, "frame-buffer-adr" );
	add_token( 0x163, "screen-height" );
	add_token( 0x164, "screen-width" );
	add_token( 0x165, "window-top" );
	add_token( 0x166, "window-left" );
	add_token( 0x16a, "default-font" );
	add_token( 0x16b, "set-font" );
	add_token( 0x16c, "char-height" );
	add_token( 0x16d, "char-width" );
	add_token( 0x16e, ">font" );
	add_token( 0x16f, "fontbytes" );
	add_token( 0x170, "fb1-draw-character" );
	add_token( 0x171, "fb1-reset-screen" );
	add_token( 0x172, "fb1-toggle-cursor" );
	add_token( 0x173, "fb1-erase-screen" );
	add_token( 0x174, "fb1-blink-screen" );
	add_token( 0x175, "fb1-invert-screen" );
	add_token( 0x176, "fb1-insert-characters" );
	add_token( 0x177, "fb1-delete-characters" );
	add_token( 0x178, "fb1-insert-lines" );
	add_token( 0x179, "fb1-delete-lines" );
	add_token( 0x17a, "fb1-draw-logo" );
	add_token( 0x17b, "fb1-install" );
	add_token( 0x17c, "fb1-slide-up" );
	add_token( 0x180, "fb8-draw-character" );
	add_token( 0x181, "fb8-reset-screen" );
	add_token( 0x182, "fb8-toggle-cursor" );
	add_token( 0x183, "fb8-erase-screen" );
	add_token( 0x184, "fb8-blink-screen" );
	add_token( 0x185, "fb8-invert-screen" );
	add_token( 0x186, "fb8-insert-characters" );
	add_token( 0x187, "fb8-delete-characters" );
	add_token( 0x188, "fb8-insert-lines" );
	add_token( 0x189, "fb8-delete-lines" );
	add_token( 0x18a, "fb8-draw-logo" );
	add_token( 0x18b, "fb8-install" );
	add_token( 0x1a0, "return-buffer" );
	add_token( 0x1a1, "xmit-packet" );
	add_token( 0x1a2, "poll-packet" );
	add_token( 0x1a4, "mac-address" );
	add_token( 0x201, "device-name" );
	add_token( 0x202, "my-args" );
	add_token( 0x203, "my-self" );
	add_token( 0x204, "find-package" );
	add_token( 0x205, "open-package" );
	add_token( 0x206, "close-package" );
	add_token( 0x207, "find-method" );
	add_token( 0x208, "call-package" );
	add_token( 0x209, "$call-parent" );
/*	add_token( 0x20a, "my-package" ); */
	add_token( 0x20a, "my-parent" );
	add_token( 0x20b, "ihandle>phandle" );
	add_token( 0x20d, "my-unit" );
	add_token( 0x20e, "$call-method" );
	add_token( 0x20f, "$open-package" );
	add_token( 0x210, "processor-type" );
	add_token( 0x211, "firmware-version" );
	add_token( 0x212, "fcode-version" );
	add_token( 0x213, "alarm" );
	add_token( 0x214, "(is-user-word)" );
	add_token( 0x215, "suspend-fcode" );
	add_token( 0x216, "abort" );
	add_token( 0x217, "catch" );
	add_token( 0x218, "throw" );
	add_token( 0x219, "user-abort" );
	add_token( 0x21a, "get-my-property" );
	add_token( 0x21b, "decode-int" );
	add_token( 0x21c, "decode-string" );
	add_token( 0x21d, "get-inherited-property" );
	add_token( 0x21e, "delete-property" );
	add_token( 0x21f, "get-package-property" );
	add_token( 0x220, "cpeek" );
	add_token( 0x221, "wpeek" );
	add_token( 0x222, "lpeek" );
	add_token( 0x223, "cpoke" );
	add_token( 0x224, "wpoke" );
	add_token( 0x225, "lpoke" );
	add_token( 0x229, "adr-mask" );
	add_token( 0x230, "rb@" );
	add_token( 0x231, "rb!" );
	add_token( 0x232, "rw@" );
	add_token( 0x233, "rw!" );
	add_token( 0x234, "rl@" );
	add_token( 0x235, "rl!" );
	add_token( 0x236, "wbflips" );
	add_token( 0x237, "lwflips" );
	add_token( 0x238, "probe" );
	add_token( 0x239, "probe-virtual" );
	add_token( 0x23b, "child" );
	add_token( 0x23c, "peer" );
	add_token( 0x240, "left-parse-string" );

	/* FCodes from 16-color Text Extension draft */
	add_token( 0x168, "foreground-color" );
	add_token( 0x169, "background-color" );

	/* FCodes from 64bit extension addendum */
	add_token( 0x22e, "rx@" );
	add_token( 0x22f, "rx!" );
	add_token( 0x241, "bxjoin" );
	add_token( 0x242, "<l@" );
	add_token( 0x243, "lxjoin" );
	add_token( 0x244, "wxjoin" );
	add_token( 0x245, "x," );
	add_token( 0x246, "x@" );
	add_token( 0x247, "x!" );
	add_token( 0x248, "/x" );
	add_token( 0x249, "/x*" );
	add_token( 0x24a, "xa+" );
	add_token( 0x24b, "xa1+" );
	add_token( 0x24c, "xbflip" );
	add_token( 0x24d, "xbflips" );
	add_token( 0x24e, "xbsplit" );
	add_token( 0x24f, "xlflip" );
	add_token( 0x250, "xlflips" );
	add_token( 0x251, "xlsplit" );
	add_token( 0x252, "xwflip" );
	add_token( 0x253, "xwflips" );
	add_token( 0x254, "xwsplit" );

	/* FCodes Added in 3.x */	
	add_token( 0x089, "unloop" );
	add_token( 0x0da, "get-token" );
	add_token( 0x0db, "set-token" );
	add_token( 0x0dc, "state" );
	add_token( 0x0dd, "compile," );
	add_token( 0x0de, "behavior" );
	add_token( 0x128, "decode-phys" );
	add_token( 0x226, "lwflip" );
	add_token( 0x227, "lbflip" );
	add_token( 0x228, "lbflips" );
	add_token( 0x23d, "next-property" );
	add_token( 0x23e, "byte-load" );
	add_token( 0x23f, "set-args" );
	add_token( 0x129, "push-package" );
	add_token( 0x12a, "pop-package" );
	add_token( 0x12b, "interpose" );

/* The names below of the compiled fcode tokens can be found in Open Firmware 2.4 or Open Firmware 4.0 or some versions of Open Firmware 3. */
/* Only the compiled fcode tokens that are included in Apple fcode drivers are included here. */

	if (mac_rom)
	{
/* tokens used by all Power Macs */

		if (mac_rom & 0x901) {
			// Open Firmware 1.0.5
			make_of_token( add_token( 0x401, "b(pushlocals)" ), "" ); /* local variables { local_0 ... local_n-1 ; ... } */
		}
		else {
			// Open Firmware 2.0 and later
			make_of_token( add_token( 0x407, "b(pushlocals)" ), "{ ; ... }" );
			add_token( 0x408, "{ local_0 ; ... }" );
			add_token( 0x409, "{ local_0 local_1 ; ... }" );
			add_token( 0x40A, "{ local_0 local_1 local_2 ; ... }" );
			add_token( 0x40B, "{ local_0 local_1 local_2 local_3 ; ... }" );
			add_token( 0x40C, "{ local_0 local_1 local_2 local_3 local_4 ; ... }" );
			add_token( 0x40D, "{ local_0 local_1 local_2 local_3 local_4 local_5 ; ... }" );
			add_token( 0x40E, "{ local_0 local_1 local_2 local_3 local_4 local_5 local_6 ; ... }" );
			add_token( 0x40F, "{ local_0 local_1 local_2 local_3 local_4 local_5 local_6 local_7 ; ... }" );
		}

		make_of_token( add_token( 0x410, "b(local@)" ), "local_0" );
		add_token( 0x411, "local_1" );
		add_token( 0x412, "local_2" );
		add_token( 0x413, "local_3" );
		add_token( 0x414, "local_4" );
		add_token( 0x415, "local_5" );
		add_token( 0x416, "local_6" );
		add_token( 0x417, "local_7" );

		make_of_token( add_token( 0x418, "b(local!)" ), "-> local_0" );
		add_token( 0x419, "-> local_1" );
		add_token( 0x41A, "-> local_2" );
		add_token( 0x41B, "-> local_3" );
		add_token( 0x41C, "-> local_4" );
		add_token( 0x41D, "-> local_5" );
		add_token( 0x41E, "-> local_6" );
		add_token( 0x41F, "-> local_7" );
	}
	
	if (mac_rom & 0x100)
	{
/* Open Firmware 0.992j; TNT Development compiled fcode tokens */
		add_token( 0x431, "(val)" );
		add_token( 0x432, "(i-val)" );
		add_token( 0x433, "b<to>" );
		add_token( 0x434, "b<to>1" );
		add_token( 0x435, "(i-to)" );
		add_token( 0x436, "(var)" );
		add_token( 0x437, "(i-var)" );
		add_token( 0x43B, "(defer)" );
		add_token( 0x43C, "(i-defer)" );
		add_token( 0x43D, "(field)" );
		add_token( 0x43E, "b<lit>" );
		add_token( 0x43F, "b<'>" );
		add_token( 0x440, "{'}" );
		add_token( 0x44F, "b<\">" );
	}

	if ((mac_rom & 0x801) == 1)
	{
/* Open Firmware 1.0.5; Power Mac 7500, 9500, 8600, etc. compiled fcode tokens */
		add_token( 0x432, "(val)" );
		add_token( 0x433, "(i-val)" );
		add_token( 0x434, "b<to>" );
		add_token( 0x435, "b<to>1" );
		add_token( 0x436, "(i-to)" );
		add_token( 0x437, "(var)" );
		add_token( 0x438, "(i-var)" );
		add_token( 0x43C, "(defer)" );
		add_token( 0x43D, "(i-defer)" );
		add_token( 0x43E, "(field)" );
		add_token( 0x43F, "b<lit>" );
		add_token( 0x440, "b<'>" );
		add_token( 0x441, "{'}" );
		add_token( 0x454, "b<\">" );
	}
	
	else if (mac_rom & 0x10)
	{
/* Open Firmware 2.0; Power Mac 5400 compiled fcode tokens */
		add_token( 0x43D, "(val)" );
		add_token( 0x43E, "(i-val)" );
		add_token( 0x43F, "b<to>" );
		add_token( 0x440, "b<to>1" );
		add_token( 0x441, "(i-to)" );
		add_token( 0x442, "(var)" );
		add_token( 0x443, "(i-var)" );
		add_token( 0x447, "(defer)" );
		add_token( 0x448, "(i-defer)" );
		add_token( 0x449, "(field)" );
		add_token( 0x44A, "b<lit>" );
		add_token( 0x44B, "b<'>" );
		add_token( 0x44C, "{'}" );
		add_token( 0x45F, "b<\">" );
	}

	else if (mac_rom & 0x200)
	{
/* Open Firmware 2.0.3; Power Mac 6500 compiled fcode tokens */
		add_token( 0x43E, "(val)" );
		add_token( 0x43F, "(i-val)" );
		add_token( 0x440, "b<to>" );
		add_token( 0x441, "b<to>1" );
		add_token( 0x442, "(i-to)" );
		add_token( 0x443, "(var)" );
		add_token( 0x444, "(i-var)" );
		add_token( 0x448, "(defer)" );
		add_token( 0x449, "(i-defer)" );
		add_token( 0x44A, "(field)" );
		add_token( 0x44B, "b<lit>" );
		add_token( 0x44C, "b<'>" );
		add_token( 0x44D, "{'}" );
		add_token( 0x461, "b<\">" );
	}

	else if (mac_rom & 0x400)
	{
/* Open Firmware 2.0.1; Power Book 3400 compiled fcode tokens, but not Kanga or Wallstreet */
		add_token( 0x43E, "(val)" );
		add_token( 0x43F, "(i-val)" );
		add_token( 0x440, "b<to>" );
		add_token( 0x441, "b<to>1" );
		add_token( 0x442, "(i-to)" );
		add_token( 0x443, "(var)" );
		add_token( 0x444, "(i-var)" );
		add_token( 0x448, "(defer)" );
		add_token( 0x449, "(i-defer)" );
		add_token( 0x44A, "(field)" );
		add_token( 0x44B, "b<lit>" );
		add_token( 0x44C, "b<'>" );
		add_token( 0x44D, "{'}" );
		add_token( 0x461, "b<\">" );
	}

	else if (mac_rom & 2)
	{
/* Open Firmware 2.0f1 and 2.4; Power Mac G3 ROM compiled fcode tokens */
		add_token( 0x43F, "(val)" );
		add_token( 0x440, "(i-val)" );
		add_token( 0x441, "b<to>" );
		add_token( 0x442, "b<to>1" );
		add_token( 0x443, "(i-to)" );
		add_token( 0x444, "(var)" );
		add_token( 0x445, "(i-var)" );
		add_token( 0x449, "(defer)" );
		add_token( 0x44A, "(i-defer)" );
		add_token( 0x44B, "(field)" );
		add_token( 0x44C, "b<lit>" );
		add_token( 0x44D, "b<'>" );
		add_token( 0x44E, "{'}" );
		add_token( 0x462, "b<\">" );
	}

	if (mac_rom & 8)
	{
/* Open Firmware 3; B&W G3 ROM compiled fcode tokens */
		add_token( 0x3fe, "lb?branch" );
		add_token( 0x3ff, "lbbranch" );

		add_token( 0x439, "(val)" );
		add_token( 0x43A, "(i-val)" );
		add_token( 0x43B, "b<to>" );
		add_token( 0x43C, "b<to>1" );
		add_token( 0x43D, "(i-to)" );
		add_token( 0x43E, "(var)" );
		add_token( 0x43F, "(i-var)" );
		add_token( 0x443, "(defer)" );
		add_token( 0x444, "(i-defer)" );
		add_token( 0x445, "(field)" );
		add_token( 0x446, "b<lit>" );
		add_token( 0x447, "b<'>" );
		add_token( 0x448, "{'}" );
		add_token( 0x458, "b<\">" );
	}

	else if (mac_rom & 0x24) {
/* Some Macs like Quad G5 and G4 Sawtooth have names for the compiled fcode tokens */
	}

	dictionaryIntialized = TRUE;
}



static void dump_one_token_as_undefined( u16 fcodeNumber )
{
	token_t * theToken;

	theToken = find_token( fcodeNumber );
	if (theToken == NULL )
		fprintf(stderr, "# Undefined 0x%03x\n", fcodeNumber );
	else if ( (theToken->flags & kFCodeDefined) == 0 )
		fprintf(stderr, "# Undefined 0x%03x \"%s\"\n", fcodeNumber, theToken->name);
}



void dump_defined(void)
{
	u16 fcodeNumber;
	token_t * theToken;
	u16 maxFcodeNumber, negativeRangeFcodeNumber;
	
	
	maxFcodeNumber = 0xFFF;
	negativeRangeFcodeNumber = 0;
	
	for (theToken=dictionary; theToken!=NULL; theToken=theToken->next)
	{
		theToken->flags &= ~kIgnoreToken;
		if ( theToken->fcode > maxFcodeNumber && theToken->fcode < 0x8000 )
			maxFcodeNumber = theToken->fcode;
		if ( theToken->fcode >= 0x8000 && ( negativeRangeFcodeNumber == 0 || theToken->fcode < negativeRangeFcodeNumber ) )
			negativeRangeFcodeNumber = theToken->fcode;
	}

	for ( fcodeNumber = 0; fcodeNumber <= maxFcodeNumber; fcodeNumber++ )
		dump_one_token_as_undefined( fcodeNumber );
	for ( fcodeNumber = negativeRangeFcodeNumber; fcodeNumber != 0; fcodeNumber++ )
		dump_one_token_as_undefined( fcodeNumber );

	if ( maxFcodeNumber > 0xFFF )
		fprintf(stderr, "# Max FCode number is 0x%03x\n", maxFcodeNumber);
	if ( negativeRangeFcodeNumber != 0 )
		fprintf(stderr, "# Min negative FCode number is 0x%04x\n", negativeRangeFcodeNumber);
}
