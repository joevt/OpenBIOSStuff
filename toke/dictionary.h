/*
 *                     OpenBIOS - free your system! 
 *                         ( FCode tokenizer )
 *                          
 *  dictionary.h - tokens for control commands.
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

#ifndef _H_DICTIONARY
#define _H_DICTIONARY

extern int mac_rom;

enum FWORD {
	COLON = 1,
	SEMICOLON,
	TOKENIZE,
	AGAIN,
	ALIAS,
	GETTOKEN,
	ASCII,
	BEGIN,
	BUFFER,
	CASE,
	CONST,
	CONTROL,
	CREATE,
	DECIMAL,
	DEFER,
	CDO,
	DO,
	ELSE,
	ENDCASE,
	ENDOF,
	EXTERNAL,
	FIELD,
	HEADERLESS,
	HEADERS,
	HEX,
	IF,
/* This is wrong and has been changed to a macro.
	CLEAVE,
*/
	LEAVE,
	CLOOP,
	LOOP,
	OCTAL,
	OF,
	REPEAT,
	THEN,
	TO,
	UNTIL,
	VALUE,
	VARIABLE,
	WHILE,
	OFFSET16,
	BEGINTOK,
	EMITBYTE,
	ENDTOK,
	FLOAD,
	STRING,
	PSTRING,
	PBSTRING,
	SSTRING,
	RECURSIVE,
	HEXVAL,
	DECVAL,
	OCTVAL,
	BINVAL,
	BINARY,
	INSTANCE,
	CODE,
	RECURSE,
	UNDEFINED_COLON,

	END0,
	END1,
	CHAR,
	CCHAR,
	ABORTTXT,

	VPDOFFSET,
	PCIARCH,
	PCIDATASTRUCTURESTART,
	PCIDATASTRUCTURELENGTH,
	ROMSIZE,
	PCIENTRY,

	NEXTFCODE,

	ENCODEFILE,
	IMPORT,

	FCODE_V1,
	FCODE_V3,
	NOTLAST,
	PCIREV,
	PCIHDR,
	PCIEND,
	START0,
	START1,
	START2,
	START4,
	VERSION1,
	FCODE_TIME,
	FCODE_DATE,
	FCODE_V2,
	FCODE_END,

	PARAMS,
	ASSIGN,
	LOCAL,
};

typedef struct token {
	char  *name;
	u16 fcode;
	u16 type;
	struct token *next;
	struct token *prev;
} token_t;

typedef struct macro {
	char  *name;
	char  *alias;
	struct macro *next;
} macro_t;

#endif
