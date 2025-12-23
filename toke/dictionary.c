/*
 *                     OpenBIOS - free your system!
 *                         ( FCode tokenizer )
 *
 *  dictionary.c - dictionary initialization and functions.
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
#if defined(__linux__) && ! defined(__USE_BSD)
#define __USE_BSD
#endif
#include <string.h>
#include <errno.h>

#ifdef __MWERKS__
#include <extras.h>

#define strdup _strdup
extern int strcasecmp(const char * str1, const char * str2);
#endif

#include "toke.h"
#include "dictionary.h"

static token_t *dictionary=NULL;
static token_t *forthwords=NULL;
static token_t *locals[8] = {0};

static token_t *lookup_token_dict0(char *name, token_t *dict)
{
	token_t *curr;
	
	for (curr=dict; curr!=NULL; curr=curr->next)
		if (curr->type != UNDEFINED_COLON && !strcasecmp(name,(char *)curr->name))
			break;

	if (curr)
		return curr;
#ifdef DEBUG_TOKE
	printf("warning: token '%s' does not exist.\n", name);
#endif
	return NULL;
}

static u16 lookup_token_dict(char *name, token_t *dict)
{
	token_t *curr = lookup_token_dict0(name, dict);
	if (curr)
		return curr->fcode;
	return 0xffff;
}

u16 lookup_token(char *name)
{
	return lookup_token_dict(name, dictionary);
}

u16 lookup_token_type(char *name)
{
	token_t *curr = lookup_token_dict0(name, dictionary);
	if (curr)
		return curr->type;
	return 0xffff;
}

u16 lookup_fword(char *name)
{
	return lookup_token_dict(name, forthwords);
}

token_t * add_token_dict(u16 number, char *name, token_t **dict, u16 type)
{
	token_t *curr;

	curr=malloc(sizeof(token_t));
	if(!curr) {
		printf("Out of memory while adding token.\n");
		exit(-ENOMEM);
	}

	curr->prev=NULL;
	curr->next=*dict;
	curr->fcode=number;
	curr->type=type;
	curr->name=name;

	if (*dict) {
		(*dict)->prev = curr;
	}

	*dict=curr;
	return curr;
}

void move_token_to_end(token_t *tok, token_t **dict)
{
	if (tok && tok->prev) {
		tok->prev->next = tok->next;

		if (tok->next)
			tok->next->prev = tok->prev;

		tok->prev = NULL;
		tok->next = *dict;

		(*dict)->prev = tok;
		*dict = tok;
	}
}

token_t * add_token(u16 number, char *name)
{
	return add_token_dict(number, name, &dictionary, 0);
}

token_t * add_token_with_type(u16 number, char *name, u16 type)
{
	return add_token_dict(number, name, &dictionary, type);
}

static token_t * add_special(u16 number, char *name)
{
	return add_token_dict(number, name, &forthwords, 0);
}

const char *token_type_string(token_t *tok)
{
	static char buff[20];
	switch (tok->type)
	{
		case COLON    : return "COLON";
		case CODE     : return "CODE";
		case CONST    : return "CONST";
		case VARIABLE : return "VARIABLE";
		case DEFER    : return "DEFER";
		case VALUE    : return "VALUE";
		default:
			sprintf(buff, "Unknown 0x%X", tok->type);
			return buff;
	}
}

void mark_defined(token_t * tok)
{
	if (tok->type == UNDEFINED_COLON) {
		tok->type = COLON;
	} else if (tok->type == COLON) {
		printf("warning: colon word 0x%03x \"%s\" is already defined.\n", tok->fcode, tok->name);
	} else {
		printf("warning: word 0x%03x \"%s\" (type %s) is not a colon word.\n", tok->fcode, tok->name, token_type_string(tok));
	}
}

void init_dictionary(void) 
{
	add_token_with_type( 0x000, "end0", COLON ); // b(end0) in Mac ROM
	add_token_with_type( 0x010, "b(lit)", COLON );
	add_token_with_type( 0x011, "b(')", COLON );
	add_token_with_type( 0x012, "b(\")", COLON );
	add_token_with_type( 0x013, "bbranch", COLON );
	add_token_with_type( 0x014, "b?branch", COLON );
	add_token_with_type( 0x015, "b(loop)", COLON );
	add_token_with_type( 0x016, "b(+loop)", COLON );
	add_token_with_type( 0x017, "b(do)", COLON );
	add_token_with_type( 0x018, "b(?do)", COLON );
	add_token_with_type( 0x019, "i", CODE );
	add_token_with_type( 0x01a, "j", CODE );
	add_token_with_type( 0x01b, "b(leave)", COLON );
	add_token_with_type( 0x01c, "b(of)", COLON );
	add_token_with_type( 0x01d, "execute", CODE );
	add_token_with_type( 0x01e, "+", CODE );
	add_token_with_type( 0x01f, "-", CODE );
	add_token_with_type( 0x020, "*", CODE );
	add_token_with_type( 0x021, "/", CODE );
	add_token_with_type( 0x022, "mod", CODE );
	add_token_with_type( 0x023, "and", CODE );
	add_token_with_type( 0x024, "or", CODE );
	add_token_with_type( 0x025, "xor", CODE );
	add_token_with_type( 0x026, "invert", CODE );
	add_token_with_type( 0x027, "<<", CODE ); // alias lshift (same fcode)
	add_token_with_type( 0x027, "lshift", CODE );
	add_token_with_type( 0x028, ">>", CODE ); // alias rshift (same fcode)
	add_token_with_type( 0x028, "rshift", CODE );
	add_token_with_type( 0x029, ">>a", CODE );
	add_token_with_type( 0x02a, "/mod", CODE );
	add_token_with_type( 0x02b, "u/mod", CODE );
	add_token_with_type( 0x02c, "negate", CODE );
	add_token_with_type( 0x02d, "abs", CODE );
	add_token_with_type( 0x02e, "min", CODE );
	add_token_with_type( 0x02f, "max", CODE );
	add_token_with_type( 0x030, ">r", CODE );
	add_token_with_type( 0x031, "r>", CODE );
	add_token_with_type( 0x032, "r@", CODE );
	add_token_with_type( 0x033, "exit", CODE );
	add_token_with_type( 0x034, "0=", CODE );
	add_token_with_type( 0x035, "0<>", CODE );
	add_token_with_type( 0x036, "0<", CODE );
	add_token_with_type( 0x037, "0<=", CODE );
	add_token_with_type( 0x038, "0>", CODE );
	add_token_with_type( 0x039, "0>=", CODE );
	add_token_with_type( 0x03a, "<", CODE );
	add_token_with_type( 0x03b, ">", CODE );
	add_token_with_type( 0x03c, "=", CODE );
	add_token_with_type( 0x03d, "<>", CODE );
	add_token_with_type( 0x03e, "u>", CODE );
	add_token_with_type( 0x03f, "u<=", CODE );
	add_token_with_type( 0x040, "u<", CODE );
	add_token_with_type( 0x041, "u>=", CODE );
	add_token_with_type( 0x042, ">=", CODE );
	add_token_with_type( 0x043, "<=", CODE );
	add_token_with_type( 0x044, "between", COLON );
	add_token_with_type( 0x045, "within", CODE );
	add_token_with_type( 0x046, "drop", CODE );
	add_token_with_type( 0x047, "dup", CODE );
	add_token_with_type( 0x048, "over", CODE );
	add_token_with_type( 0x049, "swap", CODE );
	add_token_with_type( 0x04a, "rot", CODE );
	add_token_with_type( 0x04b, "-rot", CODE );
	add_token_with_type( 0x04c, "tuck", CODE );
	add_token_with_type( 0x04d, "nip", CODE );
	add_token_with_type( 0x04e, "pick", CODE );
	add_token_with_type( 0x04f, "roll", COLON );
	add_token_with_type( 0x050, "?dup", CODE );
	add_token_with_type( 0x051, "depth", CODE );
	add_token_with_type( 0x052, "2drop", CODE );
	add_token_with_type( 0x053, "2dup", CODE );
	add_token_with_type( 0x054, "2over", CODE );
	add_token_with_type( 0x055, "2swap", CODE );
	add_token_with_type( 0x056, "2rot", CODE );
	add_token_with_type( 0x057, "2/", CODE );
	add_token_with_type( 0x058, "u2/", CODE );
	add_token_with_type( 0x059, "2*", CODE );
	add_token_with_type( 0x05a, "/c", CONST );
	add_token_with_type( 0x05b, "/w", CONST );
	add_token_with_type( 0x05c, "/l", CONST );
	add_token_with_type( 0x05d, "/n", CONST );
	add_token_with_type( 0x05e, "ca+", CODE );
	add_token_with_type( 0x05f, "wa+", CODE );
	add_token_with_type( 0x060, "la+", CODE );
	add_token_with_type( 0x061, "na+", CODE ); // alias la+
	add_token_with_type( 0x062, "char+", CODE );
	add_token_with_type( 0x063, "wa1+", CODE );
	add_token_with_type( 0x064, "la1+", CODE ); // alias cell+
	add_token_with_type( 0x065, "cell+", CODE );
	add_token_with_type( 0x066, "chars", CODE );
	add_token_with_type( 0x067, "/w*", CODE ); // alias 2*
	add_token_with_type( 0x068, "/l*", CODE ); // alias cells
	add_token_with_type( 0x069, "cells", CODE );
	add_token_with_type( 0x06a, "on", CODE );
	add_token_with_type( 0x06b, "off", CODE );
	add_token_with_type( 0x06c, "+!", CODE );
	add_token_with_type( 0x06d, "@", CODE );
	add_token_with_type( 0x06e, "l@", CODE ); // alias @
	add_token_with_type( 0x06f, "w@", CODE );
	add_token_with_type( 0x070, "<w@", CODE );
	add_token_with_type( 0x071, "c@", CODE );
	add_token_with_type( 0x072, "!", CODE );
	add_token_with_type( 0x073, "l!", CODE ); // alias !
	add_token_with_type( 0x074, "w!", CODE );
	add_token_with_type( 0x075, "c!", CODE );
	add_token_with_type( 0x076, "2@", CODE );
	add_token_with_type( 0x077, "2!", CODE );
	add_token_with_type( 0x078, "move", CODE );
	add_token_with_type( 0x079, "fill", CODE );
	add_token_with_type( 0x07a, "comp", CODE );
	add_token_with_type( 0x07b, "noop", COLON ); // b(noop) in Mac ROM
	add_token_with_type( 0x07c, "lwsplit", CODE );
	add_token_with_type( 0x07d, "wljoin", CODE );
	add_token_with_type( 0x07e, "lbsplit", CODE );
	add_token_with_type( 0x07f, "bljoin", CODE );
	add_token_with_type( 0x080, "wbflip", CODE );
	add_token_with_type( 0x081, "upc", COLON );
	add_token_with_type( 0x082, "lcc", COLON );
	add_token_with_type( 0x083, "pack", CODE );
	add_token_with_type( 0x084, "count", CODE );
	add_token_with_type( 0x085, "body>", COLON );
	add_token_with_type( 0x086, ">body", COLON );
	add_token_with_type( 0x087, "fcode-revision", COLON );
	add_token_with_type( 0x088, "span", VARIABLE );
	add_token_with_type( 0x089, "unloop", CODE );
	add_token_with_type( 0x08a, "expect", DEFER );
	add_token_with_type( 0x08b, "alloc-mem", DEFER );
	add_token_with_type( 0x08c, "free-mem", DEFER );
	add_token_with_type( 0x08d, "key?", DEFER );
	add_token_with_type( 0x08e, "key", DEFER );
	add_token_with_type( 0x08f, "emit", DEFER );
	add_token_with_type( 0x090, "type", DEFER );
	add_token_with_type( 0x091, "(cr", COLON );
	add_token_with_type( 0x092, "cr", COLON );
	add_token_with_type( 0x093, "#out", VARIABLE );
	add_token_with_type( 0x094, "#line", VARIABLE );
	add_token_with_type( 0x095, "hold", COLON );
	add_token_with_type( 0x096, "<#", COLON );
	add_token_with_type( 0x097, "u#>", COLON );
	add_token_with_type( 0x098, "sign", COLON );
	add_token_with_type( 0x099, "u#", COLON );
	add_token_with_type( 0x09a, "u#s", COLON );
	add_token_with_type( 0x09b, "u.", COLON );
	add_token_with_type( 0x09c, "u.r", COLON );
	add_token_with_type( 0x09d, ".", DEFER );
	add_token_with_type( 0x09e, ".r", COLON );
	add_token_with_type( 0x09f, ".s", COLON );
	add_token_with_type( 0x0a0, "base", VARIABLE );
	add_token( 0x0a1, "convert" ); // historical
	add_token_with_type( 0x0a2, "$number", COLON );
	add_token_with_type( 0x0a3, "digit", COLON );
	add_token_with_type( 0x0a4, "-1", CODE );
	add_token_with_type( 0x0a5, "0", CODE );
	add_token_with_type( 0x0a6, "1", CODE );
	add_token_with_type( 0x0a7, "2", CODE );
	add_token_with_type( 0x0a8, "3", CODE );
	add_token_with_type( 0x0a9, "bl", CODE );
	add_token_with_type( 0x0aa, "bs", CODE );
	add_token_with_type( 0x0ab, "bell", CODE );
	add_token_with_type( 0x0ac, "bounds", COLON );
	add_token_with_type( 0x0ad, "here", CODE );
	add_token_with_type( 0x0ae, "aligned", CODE );
	add_token_with_type( 0x0af, "wbsplit", CODE );
	add_token_with_type( 0x0b0, "bwjoin", CODE );
	add_token_with_type( 0x0b1, "b(<mark)", COLON );
	add_token_with_type( 0x0b2, "b(>resolve)", COLON );
	add_token( 0x0b3, "set-token-table" ); // historical; set-token in spec
	add_token( 0x0b4, "set-table" ); // historical
	add_token_with_type( 0x0b5, "new-token", COLON );
	add_token_with_type( 0x0b6, "named-token", COLON );
	add_token_with_type( 0x0b7, "b(:)", COLON );
	add_token_with_type( 0x0b8, "b(value)", COLON );
	add_token_with_type( 0x0b9, "b(variable)", COLON );
	add_token_with_type( 0x0ba, "b(constant)", COLON );
	add_token_with_type( 0x0bb, "b(create)", COLON );
	add_token_with_type( 0x0bc, "b(defer)", COLON );
	add_token_with_type( 0x0bd, "b(buffer:)", COLON );
	add_token_with_type( 0x0be, "b(field)", COLON );
	add_token( 0x0bf, "b(code)" ); // historical
	add_token_with_type( 0x0c0, "instance", COLON );
	add_token_with_type( 0x0c2, "b(;)", COLON );
	add_token_with_type( 0x0c3, "b(to)", COLON );
	add_token_with_type( 0x0c4, "b(case)", COLON );
	add_token_with_type( 0x0c5, "b(endcase)", COLON );
	add_token_with_type( 0x0c6, "b(endof)", COLON );
	add_token_with_type( 0x0c7, "#", COLON );
	add_token_with_type( 0x0c8, "#s", COLON );
	add_token_with_type( 0x0c9, "#>", COLON );
	add_token_with_type( 0x0ca, "external-token", COLON );
	add_token_with_type( 0x0cb, "$find", COLON );
	add_token_with_type( 0x0cc, "offset16", COLON );
	add_token_with_type( 0x0cd, "evaluate", COLON );
	add_token_with_type( 0x0d0, "c,", COLON );
	add_token_with_type( 0x0d1, "w,", COLON );
	add_token_with_type( 0x0d2, "l,", COLON );
	add_token_with_type( 0x0d3, ",", COLON );
	add_token_with_type( 0x0d4, "um*", CODE );
	add_token_with_type( 0x0d5, "um/mod", CODE );
	add_token_with_type( 0x0d8, "d+", CODE );
	add_token_with_type( 0x0d9, "d-", CODE );
	add_token_with_type( 0x0da, "get-token", CODE );
	add_token_with_type( 0x0db, "set-token", CODE );
	add_token_with_type( 0x0dc, "state", VARIABLE );
	add_token_with_type( 0x0dd, "compile,", COLON );
	add_token_with_type( 0x0de, "behavior", COLON );
	add_token_with_type( 0x0f0, "start0", COLON );
	add_token_with_type( 0x0f1, "start1", COLON );
	add_token_with_type( 0x0f2, "start2", COLON );
	add_token_with_type( 0x0f3, "start4", COLON );
	add_token_with_type( 0x0fc, "ferror", COLON );
	add_token_with_type( 0x0fd, "version1", COLON );
	add_token( 0x0fe, "4-byte-id" ); // historical
	add_token_with_type( 0x0ff, "end1", COLON ); // b(end1) in Mac ROM
	add_token( 0x101, "dma-alloc" ); // historical
	add_token_with_type( 0x102, "my-address", COLON );
	add_token_with_type( 0x103, "my-space", COLON );
	add_token( 0x104, "memmap" ); // historical
	add_token_with_type( 0x105, "free-virtual", COLON );
	add_token( 0x106, ">physical" ); // historical
	add_token( 0x10f, "my-params" ); // historical
	add_token_with_type( 0x110, "property", COLON );
	add_token_with_type( 0x111, "encode-int", COLON );
	add_token_with_type( 0x112, "encode+", COLON );
	add_token_with_type( 0x113, "encode-phys", COLON );
	add_token_with_type( 0x114, "encode-string", COLON );
	add_token_with_type( 0x115, "encode-bytes", COLON );
	add_token_with_type( 0x116, "reg", COLON );
	add_token( 0x117, "intr" ); // not in Mac ROM
	add_token( 0x118, "driver" ); // historical
	add_token_with_type( 0x119, "model", COLON );
	add_token_with_type( 0x11a, "device-type", COLON );
	add_token_with_type( 0x11b, "parse-2int", COLON );
	add_token_with_type( 0x11c, "is-install", COLON );
	add_token_with_type( 0x11d, "is-remove", COLON );
	add_token( 0x11e, "is-selftest" ); // not in Mac ROM
	add_token_with_type( 0x11f, "new-device", COLON );
	add_token_with_type( 0x120, "diagnostic-mode?", COLON );
	add_token( 0x121, "display-status" ); // not in Mac ROM
	add_token( 0x122, "memory-test-issue" ); // memory-test-suite in Mac ROM
	add_token( 0x123, "group-code" ); // historical
	add_token_with_type( 0x124, "mask", VARIABLE );
	add_token_with_type( 0x125, "get-msecs", DEFER );
	add_token_with_type( 0x126, "ms", COLON );
	add_token_with_type( 0x127, "finish-device", COLON );
	add_token_with_type( 0x128, "decode-phys", COLON );
//	add_token_with_type( 0x129, "push-package", COLON ); // from Mac ROM
//	add_token_with_type( 0x12a, "pop-package", COLON ); // from Mac ROM
	add_token_with_type( 0x12b, "interpose", COLON );
	add_token_with_type( 0x130, "map-low", COLON );
	add_token( 0x131, "sbus-intr>cpu" ); // not in Mac ROM
	add_token_with_type( 0x150, "#lines", VALUE );
	add_token_with_type( 0x151, "#columns", VALUE );
	add_token_with_type( 0x152, "line#", VALUE );
	add_token_with_type( 0x153, "column#", VALUE );
	add_token_with_type( 0x154, "inverse?", VALUE );
	add_token_with_type( 0x155, "inverse-screen?", VALUE );
	add_token( 0x156, "frame-buffer-busy?" ); // not in Mac ROM
	add_token_with_type( 0x157, "draw-character", DEFER );
	add_token_with_type( 0x158, "reset-screen", DEFER );
	add_token_with_type( 0x159, "toggle-cursor", DEFER );
	add_token_with_type( 0x15a, "erase-screen", DEFER );
	add_token_with_type( 0x15b, "blink-screen", DEFER );
	add_token_with_type( 0x15c, "invert-screen", DEFER );
	add_token_with_type( 0x15d, "insert-characters", DEFER );
	add_token_with_type( 0x15e, "delete-characters", DEFER );
	add_token_with_type( 0x15f, "insert-lines", DEFER );
	add_token_with_type( 0x160, "delete-lines", DEFER );
	add_token_with_type( 0x161, "draw-logo", DEFER );
	add_token_with_type( 0x162, "frame-buffer-adr", VALUE);
	add_token_with_type( 0x163, "screen-height", VALUE);
	add_token_with_type( 0x164, "screen-width", VALUE);
	add_token_with_type( 0x165, "window-top", VALUE);
	add_token_with_type( 0x166, "window-left", VALUE);
	add_token_with_type( 0x168, "foreground-color", VALUE ); // from Mac ROM (also 16-color Text Extension)
	add_token_with_type( 0x169, "background-color", VALUE ); // from Mac ROM (also 16-color Text Extension)
	add_token_with_type( 0x16a, "default-font", COLON );
	add_token_with_type( 0x16b, "set-font", COLON );
	add_token_with_type( 0x16c, "char-height", VALUE);
	add_token_with_type( 0x16d, "char-width", VALUE);
	add_token_with_type( 0x16e, ">font", COLON );
	add_token_with_type( 0x16f, "fontbytes", VALUE);
	add_token( 0x170, "fb1-draw-character" ); // historical
	add_token( 0x171, "fb1-reset-screen" ); // historical
	add_token( 0x172, "fb1-toggle-cursor" ); // historical
	add_token( 0x173, "fb1-erase-screen" ); // historical
	add_token( 0x174, "fb1-blink-screen" ); // historical
	add_token( 0x175, "fb1-invert-screen" ); // historical
	add_token( 0x176, "fb1-insert-characters" ); // historical
	add_token( 0x177, "fb1-delete-characters" ); // historical
	add_token( 0x178, "fb1-insert-lines" ); // historical
	add_token( 0x179, "fb1-delete-lines" ); // historical
	add_token( 0x17a, "fb1-draw-logo" ); // historical
	add_token( 0x17b, "fb1-install" ); // historical
	add_token( 0x17c, "fb1-slide-up" ); // historical
	add_token_with_type( 0x180, "fb8-draw-character", COLON );
	add_token_with_type( 0x181, "fb8-reset-screen", COLON );
	add_token_with_type( 0x182, "fb8-toggle-cursor", COLON );
	add_token_with_type( 0x183, "fb8-erase-screen", COLON );
	add_token_with_type( 0x184, "fb8-blink-screen", COLON );
	add_token_with_type( 0x185, "fb8-invert-screen", COLON );
	add_token_with_type( 0x186, "fb8-insert-characters", COLON );
	add_token_with_type( 0x187, "fb8-delete-characters", COLON );
	add_token_with_type( 0x188, "fb8-insert-lines", COLON );
	add_token_with_type( 0x189, "fb8-delete-lines", COLON );
	add_token_with_type( 0x18a, "fb8-draw-logo", COLON );
	add_token_with_type( 0x18b, "fb8-install", COLON );
	add_token( 0x1a0, "return-buffer" ); // historical
	add_token( 0x1a1, "xmit-packet" ); // historical
	add_token( 0x1a2, "poll-packet" ); // historical
	add_token_with_type( 0x1a4, "mac-address", COLON );
	add_token_with_type( 0x201, "device-name", COLON );
	add_token_with_type( 0x202, "my-args", COLON );
	add_token_with_type( 0x203, "my-self", VALUE );
	add_token_with_type( 0x204, "find-package", COLON );
	add_token_with_type( 0x205, "open-package", DEFER );
	add_token_with_type( 0x206, "close-package", COLON );
	add_token_with_type( 0x207, "find-method", COLON );
	add_token_with_type( 0x208, "call-package", COLON );
	add_token_with_type( 0x209, "$call-parent", COLON );
	add_token_with_type( 0x20a, "my-parent", COLON ); /* This is the correct token for 0x20a. Not sure where my-package comes from. */
	add_token_with_type( 0x20a, "my-package", COLON ); // not in Mac ROM
	add_token_with_type( 0x20b, "ihandle>phandle", COLON );
	add_token_with_type( 0x20d, "my-unit", COLON );
	add_token_with_type( 0x20e, "$call-method", COLON );
	add_token_with_type( 0x20f, "$open-package", COLON );
	add_token( 0x210, "processor-type" ); // historical
	add_token( 0x211, "firmware-version" ); // historical
	add_token( 0x212, "fcode-version" ); // historical
	add_token_with_type( 0x213, "alarm", COLON );
	add_token_with_type( 0x214, "(is-user-word)", COLON );
	add_token_with_type( 0x215, "suspend-fcode", COLON );
	add_token_with_type( 0x216, "abort", COLON );
	add_token_with_type( 0x217, "catch", CODE );
	add_token_with_type( 0x218, "throw", CODE );
	add_token_with_type( 0x219, "user-abort", COLON );
	add_token_with_type( 0x21a, "get-my-property", COLON );
	add_token_with_type( 0x21b, "decode-int", COLON );
	add_token_with_type( 0x21c, "decode-string", COLON );
	add_token_with_type( 0x21d, "get-inherited-property", COLON );
	add_token_with_type( 0x21e, "delete-property", COLON );
	add_token_with_type( 0x21f, "get-package-property", COLON );
	add_token_with_type( 0x220, "cpeek", COLON );
	add_token_with_type( 0x221, "wpeek", COLON );
	add_token_with_type( 0x222, "lpeek", COLON );
	add_token_with_type( 0x223, "cpoke", COLON );
	add_token_with_type( 0x224, "wpoke", COLON );
	add_token_with_type( 0x225, "lpoke", COLON );
	add_token_with_type( 0x226, "lwflip", CODE );
	add_token_with_type( 0x227, "lbflip", CODE );
	add_token_with_type( 0x228, "lbflips", CODE );
	add_token( 0x229, "adr-mask" ); // historical
	add_token_with_type( 0x230, "rb@", CODE ); // xb@ in Mac ROM
	add_token_with_type( 0x231, "rb!", CODE ); // xb! in Mac ROM
	add_token_with_type( 0x232, "rw@", CODE ); // xw@ in Mac ROM
	add_token_with_type( 0x233, "rw!", CODE ); // xw! in Mac ROM
	add_token_with_type( 0x234, "rl@", CODE ); // xl@ in Mac ROM
	add_token_with_type( 0x235, "rl!", CODE ); // xl! in Mac ROM
	add_token_with_type( 0x236, "wbflips", CODE );
	add_token_with_type( 0x237, "lwflips", CODE );
	add_token( 0x238, "probe" ); // historical
	add_token( 0x239, "probe-virtual" ); // historical
	add_token_with_type( 0x23b, "child", COLON );
	add_token_with_type( 0x23c, "peer", COLON );
	add_token_with_type( 0x23d, "next-property", COLON );
	add_token_with_type( 0x23e, "byte-load", COLON );
	add_token_with_type( 0x23f, "set-args", COLON );
	add_token_with_type( 0x240, "left-parse-string", COLON );

	/* FCodes from 64bit extension addendum */
	add_token( 0x22e, "rx@" ); // not in Mac ROM
	add_token( 0x22f, "rx!" ); // not in Mac ROM
	add_token( 0x241, "bxjoin" ); // not in Mac ROM
	add_token( 0x242, "<l@" ); // not in Mac ROM
	add_token( 0x243, "lxjoin" ); // not in Mac ROM
	add_token( 0x244, "wxjoin" ); // not in Mac ROM
	add_token( 0x245, "x," ); // not in Mac ROM
	add_token( 0x246, "x@" ); // not in Mac ROM
	add_token( 0x247, "x!" ); // not in Mac ROM
	add_token_with_type( 0x248, "/x", CONST );
	add_token_with_type( 0x249, "/x*", COLON );
	add_token_with_type( 0x24a, "xa+", CODE );
	add_token_with_type( 0x24b, "xa1+", COLON );
	add_token( 0x24c, "xbflip" ); // not in Mac ROM
	add_token_with_type( 0x24d, "xbflips", CODE );
	add_token( 0x24e, "xbsplit" ); // not in Mac ROM
	add_token( 0x24f, "xlflip" ); // not in Mac ROM
	add_token_with_type( 0x250, "xlflips", CODE );
	add_token( 0x251, "xlsplit" ); // not in Mac ROM
	add_token( 0x252, "xwflip" ); // not in Mac ROM
	add_token_with_type( 0x253, "xwflips", CODE );
	add_token( 0x254, "xwsplit" ); // not in Mac ROM

	add_special(COLON, 	":");
	add_special(SEMICOLON, 	";");
	add_special(TOKENIZE, 	"'");
	add_special(AGAIN, 	"again");
	add_special(ALIAS, 	"alias");
	add_special(GETTOKEN, 	"[']");
	add_special(ASCII, 	"ascii");
	add_special(BEGIN, 	"begin");
	add_special(BUFFER, 	"buffer:");
	add_special(CASE, 	"case");
	add_special(CONST, 	"constant");
	add_special(CONTROL, 	"control");
	add_special(CREATE, 	"create");
	add_special(DECIMAL, 	"decimal");
	add_special(DEFER, 	"defer");
	add_special(CDO, 	"?do");
	add_special(DO, 	"do");
	add_special(ELSE, 	"else");
	add_special(ENDCASE, 	"endcase");
	add_special(ENDOF, 	"endof");
	add_special(EXTERNAL, 	"external");
	add_special(INSTANCE, 	"instance");
	add_special(FIELD, 	"field");
	add_special(HEADERLESS, "headerless");
	add_special(HEADERS, 	"headers");
	add_special(HEX, 	"hex");
	add_special(IF, 	"if");
/* this is supposed to be a macro
	add_special(CLEAVE, 	"?leave");
*/
	add_special(LEAVE, 	"leave");
	add_special(CLOOP, 	"+loop");
	add_special(LOOP, 	"loop");
	add_special(OCTAL,	"octal");
	add_special(BINARY,	"binary");
	add_special(OF,		"of");
	add_special(REPEAT,	"repeat");
	add_special(THEN,	"then");
	add_special(TO,		"to");
	add_special(UNTIL,	"until");
	add_special(VALUE,	"value");
	add_special(VARIABLE,	"variable");
	add_special(WHILE,	"while");
	add_special(OFFSET16,	"offset16");
	add_special(BEGINTOK,	"tokenizer[");
	add_special(EMITBYTE,	"emit-byte");
	add_special(ENDTOK,	"]tokenizer");
	add_special(FLOAD,	"fload");
	add_special(STRING,	"\"");
	add_special(PSTRING,	".\"");
	add_special(PBSTRING,	".(");
	add_special(SSTRING,	"s\"");
	add_special(RECURSIVE,	"recursive");
	add_special(RECURSE,	"recurse");
	add_special(NEXTFCODE,	"next-fcode");
	/* version1 is also an fcode word, but it 
	 * needs to trigger some tokenizer internals */
	add_special(VERSION1,	"version1");
	add_special(START0,	"start0");
	add_special(START1,	"start1");
	add_special(START2,	"start2");
	add_special(START4,	"start4");
	add_special(END0,	"end0");
	add_special(END1,	"end1");
	add_special(FCODE_V1,	"fcode-version1");
	add_special(FCODE_V2,	"fcode-version2");
	add_special(FCODE_V3,	"fcode-version3");
	add_special(FCODE_END,	"fcode-end");
	add_special(FCODE_DATE,	"fcode-date");
	add_special(FCODE_TIME,	"fcode-time");
	
	add_special(HEXVAL,	"h#");
	add_special(DECVAL,	"d#");
	add_special(OCTVAL,	"o#");
	add_special(BINVAL,	"b#");
	add_special(CHAR,	"char");
	add_special(CCHAR,	"[char]");
	add_special(ABORTTXT,   "abort\"");

	add_special(ENCODEFILE,	"encode-file");

	/* pci header generation is done differently 
	 * across the available tokenizers. We try to
	 * be compatible to all of them
	 */
	add_special(PCIHDR,	"pci-header");
	add_special(PCIEND,	"pci-end");            /* SUN syntax */
	add_special(PCIEND,	"pci-header-end");     /* Firmworks syntax */
	add_special(PCIREV,	"pci-revision");       /* SUN syntax */
	add_special(PCIREV,	"pci-code-revision");  /* SUN syntax */
	add_special(PCIREV,	"set-rev-level");      /* Firmworks syntax */
	add_special(NOTLAST,	"not-last-image");
	add_special(PCIARCH,	"pci-architecture");
	add_special(PCIDATASTRUCTURESTART,	"pci-data-structure-start");
	add_special(PCIDATASTRUCTURELENGTH,	"pci-data-structure-length");
	add_special(VPDOFFSET,	"set-vpd-offset");
	add_special(ROMSIZE,	"rom-size");
	add_special(PCIENTRY, "pci-entry");

	if (mac_rom) {
		add_special(PARAMS, "{" );
		add_special(ASSIGN, "->" );
		locals[0] = add_token_with_type( 0x410, "", LOCAL );
		locals[1] = add_token_with_type( 0x411, "", LOCAL );
		locals[2] = add_token_with_type( 0x412, "", LOCAL );
		locals[3] = add_token_with_type( 0x413, "", LOCAL );
		locals[4] = add_token_with_type( 0x414, "", LOCAL );
		locals[5] = add_token_with_type( 0x415, "", LOCAL );
		locals[6] = add_token_with_type( 0x416, "", LOCAL );
		locals[7] = add_token_with_type( 0x417, "", LOCAL );
	}
}

void reset_locals(void)
{
	for (int i = 0; i < 8; i++)
		set_local(i, "");
}

void set_local(int i, char* name)
{
	if (locals[i]) {
		if (locals[i]->name && locals[i]->name[0])
			free(locals[i]->name);
		locals[i]->name = name;
		move_token_to_end(locals[i], &dictionary);
	}
}
