/*
 *                     OpenBIOS - free your system!
 *                         ( FCode tokenizer )
 *
 *  scanner.c - simple scanner for forth files.
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
#include <unistd.h>
#ifdef __GLIBC__
#define __USE_XOPEN_EXTENDED
#endif
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef __MWERKS__
#include <extras.h>

#define DEBUG_SCANNER 0

/*
#define strdup _strdup

int strcasecmp(const char * str1, const char * str2)
{
	const unsigned char * p1 = (unsigned char *) str1 - 1;
	const unsigned char * p2 = (unsigned char *) str2 - 1;
	unsigned long c1, c2;
	while ((c1 = toupper(*++p1)) == (c2 = toupper(*++p2)))
		if (!c1)
			return(0);
	return(c1 - c2);
}
*/

#endif

#include "toke.h"
#include "stack.h"
#include "stream.h"
#include "emit.h"
#include "dictionary.h"

#define ERROR    do { if (!noerrors) exit(-1);  } while (0)

extern u8 *start, *pc, *end, *opc, *ostart;
extern int verbose, noerrors;
extern int lowercase;
extern bool gGotError;

u8   *statbuf=NULL;
u16  nextfcode;
u8   base=0x0a;

/* header pointers */
u8  *pci_hdr;

/* pci data */
bool pci_is_last_image;
bool pci_want_header;
u16  pci_revision;
u16  pci_vpd;
bool got_processor_architecture;
u16  processor_architecture;
u16  pci_data_structure_start;
u16  pci_data_structure_length;
u32  rom_size = 0;
bool got_pci_entry;
u32 pci_entry;

void pci_reset( void ) {
	pci_hdr=NULL;
	pci_is_last_image=TRUE;
	pci_want_header=FALSE;
	pci_revision=0x0001;
	pci_vpd=0x0000;
	got_processor_architecture=FALSE;
	processor_architecture=0;
	pci_data_structure_start=0x001C;
	pci_data_structure_length=0x0018;
	got_pci_entry=FALSE;
	pci_entry=0;
}

bool offs16=TRUE;
static bool intok=FALSE, in_to=FALSE, in_instance=FALSE;
static token_t * incolon=NULL;
bool haveend=FALSE;

static u8 *skipws(u8 *str)
{
	if (str)
		while (*str=='\t' || *str==' ' || *str=='\n' ) {
			if (*str=='\n')
				lineno++;
			str++;
		}

	return str;
}

static u8 *firstchar(u8 needle, u8 *str)
{
	if (str)
		while (*str && *str!=needle) {
			if (*str=='\n')
				lineno++;
			str++;
		}

	return str;
}

static unsigned long get_word(void)
{
	size_t len;
	u8 *str;

	pc=skipws(pc);
	if (pc>=end)
		return 0;

	str=pc;
	if (str)
		while (*str && *str!='\n' && *str!='\t' && *str!=' ')
			str++;

	len=(size_t)(str-pc);
	if (len>1023) {
		printf(FILE_POSITION "error: buffer overflow.\n", iname, lineno);
		ERROR;
	}

	memcpy(statbuf, pc, len);
	statbuf[len]=0;

#if DEBUG_SCANNER
	printf(FILE_POSITION "debug: read token '%s', length=%ld\n",
			iname, lineno, statbuf, len);
#endif
	pc+=len;
	return len;
}

static unsigned long get_until(char needle)
{
	u8 *safe;
	unsigned long len;

	safe=pc;
	pc=firstchar(needle,safe);
	if (!pc || pc>=end)
		return 0;

	len=(unsigned long)pc-(unsigned long)safe;
	if (*pc++=='\n') lineno++; /* skip over needle character */
	if (len>1023) {
		printf(FILE_POSITION "error: buffer overflow\n", iname, lineno);
		ERROR;
	}

	memcpy(statbuf, safe, len);
	statbuf[len]=0;
	return len;
}

static long parse_number(u8 *start, u8 **endptr, int lbase)
{
	long val = 0;
	int negative = 0, curr;
	u8 *nptr=start;
	int got_digit = 0;

	curr = *nptr;
	if (curr == '-') {
		negative=1;
		nptr++;
	}

	for (; (curr = *nptr); nptr++) {
		if ( curr == '.' )
			continue;
		if ( curr >= '0' && curr <= '9')
			curr -= '0';
		else if (curr >= 'a' && curr <= 'f')
			curr += 10 - 'a';
		else if (curr >= 'A' && curr <= 'F')
			curr += 10 - 'A';
		else
			break;

		if (curr >= lbase)
			break;

		got_digit = 1;
		val *= lbase;
		val += curr;
	}

#if DEBUG_SCANNER
	if (curr || !got_digit)
		printf(FILE_POSITION "warning: couldn't parse number '%s' (%d/%d)\n",
				iname, lineno, start,curr,lbase);
#endif

	if (!got_digit)
		nptr = start;
	if (endptr)
		*endptr=nptr;

	if (negative)
		return -val;
	return val;
}

static u8 *get_sequence(u8 *walk)
{
	u8 val, pval[3];

	pc++; /* skip the ( */
	#if DEBUG_SCANNER
		printf(FILE_POSITION "debug: hex field:", iname, lineno);
	#endif
	pval[1]=0; pval[2]=0;

	for(;;) {
		pc=skipws(pc);

		pval[0]=*pc;
		if (pval[0]==')')
			break;

		pc++; /* this cannot be a \n */

		pval[1]=*pc;
		if ( *pc++=='\n' )
			lineno++;

		val=parse_number(pval, NULL, 16);
		*(walk++)=val;
		#if DEBUG_SCANNER
			printf(" %02x",val);
		#endif

		if (pval[1]==')')
			break;
	}
	#if DEBUG_SCANNER
		printf("\n");
	#endif

	return walk;
}

static unsigned long get_string(void)
{
	u8 *walk;
	unsigned long len;
	bool run=1;
	u8 c;
	bool gotstring=0;

	walk=statbuf;
	while (1) {
		switch ((c=*pc)) {
#if 0 /* Mac Open Firmware does not use back-slash */
			u8 val;

			case '\\':
				switch ((c=*(++pc))) {
					case 'n':
						/* newline */
						*(walk++)='\n';
						break;
					case 't':
						/* tab */
						*(walk++)='\t';
						break;
					default:
						val=parse_number(pc, &pc, base);
						#if DEBUG_SCANNER
							if (verbose)
								printf(FILE_POSITION "debug: escape code "
									"0x%x\n",iname, lineno, val);
						#endif
						*(walk++)=val;
				}
				break;
#endif
			case '\"':
				pc++; /* skip the " */

				/* printf("switching: %c\n",*pc); */
				switch((c=*pc)) {
					case '(':
						walk=get_sequence(walk);
						break;
					case '"':
						*(walk++)='"';
						break;
					case 'n':
						*(walk++)='\n';
						break;
					case 'r':
						*(walk++)='\r';
						break;
					case 't':
						*(walk++)='\t';
						break;
					case 'f':
						*(walk++)='\f';
						break;
					case 'l':
						*(walk++)='\n';
						break;
					case 'b':
						*(walk++)=0x08;
						break;
					case '!':
						*(walk++)=0x07;
						break;
					case '^':
						pc++;
						c=toupper(*pc);
						*(walk++)=c-'A';
						break;
					case '\n':
						lineno++;
					case ' ':
					case '\t':
						run=0;
						gotstring=1;
						pc++;
						break;
					default:
						*(walk++)=c;
						break;
				}
				break;
			case '\n':
				lineno++;
				break;
			case '\0':
				run=0;
				break;
			default:
				*(walk++)=c;
		}
		if (!run)
			break;
		pc++;
	}

	*(walk++)=0;

	if (!gotstring)
		return 0;

	len=(unsigned long)walk-(unsigned long)statbuf;
	if (len>1023) {
		printf(FILE_POSITION "error: buffer overflow\n", iname, lineno);
		ERROR;
	}
	#if DEBUG_SCANNER
		if (verbose)
			printf(FILE_POSITION "debug: scanned string: '%s'\n",
						iname, lineno, statbuf);
	#endif

	return len>255?255:len;
}

static int get_number(long *result)
{
	u8 lbase, *until;
	long val;

	lbase=intok?0x10:base;
	val=parse_number(statbuf, &until, lbase);

#if DEBUG_SCANNER
	printf(FILE_POSITION "debug: parsing number: base 0x%x, val 0x%lx, "
			"processed %ld of %ld bytes\n", iname, lineno,
			lbase, val,(size_t)(until-statbuf), strlen((char *)statbuf));
#endif

	if (until==(statbuf+strlen((char *)statbuf))) {
		*result=val;
		return 0;
	}

	return -1;
}

void init_scanner(void)
{
	statbuf=malloc(1024);
	if (!statbuf) {
		printf ("no memory.\n");
		exit(-1);
	}
}

void exit_scanner(void)
{
	free(statbuf);
}

#define FLAG_EXTERNAL 0x01
#define FLAG_HEADERS  0x02
char *name, *alias;
int flags=0;

static token_t * create_word(u16 tok)
{
	unsigned long wlen;
	token_t * token;

	if (incolon) {
		printf(FILE_POSITION "error: creating words not allowed "
			"in colon definition.\n", iname, lineno);
		ERROR;
	}

	wlen=get_word();
	name=strdup((char *)statbuf);

	if (lowercase)
	{
		for (char* c = name; *c; c++)
			*c = tolower( *c );
	}

#if DEBUG_SCANNER
	printf(FILE_POSITION "debug: defined new word %s, fcode no 0x%x\n",
			iname, lineno, name, nextfcode);
#endif
	token = add_token_with_type(nextfcode, name, tok);
	if (flags) {
		if (flags&FLAG_EXTERNAL)
			emit_token("external-token");
		else
			emit_token("named-token");
		emit_string((u8 *)name, wlen);
	} else
		emit_token("new-token");

	emit_fcode(nextfcode);
	nextfcode++;

	return token;
}

static void encode_file( const char *filename )
{
	FILE *f;
	size_t s;
	int i=0;

	if( !(f=fopen(filename,"rb")) ) {
		printf(FILE_POSITION "opening '%s':\n", iname, lineno, filename );
		ERROR;
		return;
	}
	while( (s=fread(statbuf, 1, 255, f)) ) {
		emit_token("b(\")");
		emit_string(statbuf, s);
		emit_token("encode-bytes");
		if( i++ )
			emit_token("encode+");
	}
	fclose( f );
}

static void validate_to_target(u16 type)
{
	if (in_to) {
		switch (type) {
		case VARIABLE:
			printf(FILE_POSITION "warning: Applying \"to\" to a VARIABLE (%s) is not recommended; use ! instead.\n",
					iname, lineno, (char *)statbuf);
			break;
		case CONST:
			printf(FILE_POSITION "warning: Applying \"to\" to a CONST (%s) is not recommended.\n",
					iname, lineno, (char *)statbuf);
			break;
		case VALUE:
		case DEFER:
			break;
		default:
			gGotError = 1;
			printf(FILE_POSITION "error: Applying \"to\" to \"%s\" is not valid.\n",
					iname, lineno, (char *)statbuf);
			break;
		}
		in_to=FALSE;
	}
}

static void validate_instance(u16 type)
{
	if (in_instance) {
		switch (type) {
		case VALUE:
		case VARIABLE:
		case DEFER:
		case BUFFER:
			break;

		default:
			gGotError = 1;
			printf(FILE_POSITION "error: Applying \"instance\" to \"%s\" is not valid.\n",
					iname, lineno, (char *)statbuf);
			break;
		}
		in_instance=FALSE;
	}
}

static void handle_internal(u16 tok)
{
	unsigned long wlen;
	long offs1,offs2;
	u16 itok;

#if DEBUG_SCANNER
	printf(FILE_POSITION "debug: tokenizing control word '%s'\n",
						iname, lineno, statbuf);
#endif
	switch (tok) {
	case BEGIN:
		emit_token("b(<mark)");
		dpushtype(kBegin, opc-ostart);
		break;

	case BUFFER:
		create_word(tok);
		emit_token("b(buffer:)");
		break;

	case CONST:
		create_word(tok);
		emit_token("b(constant)");
		break;

	case COLON:
		incolon=create_word(UNDEFINED_COLON);
		emit_token("b(:)");
		break;

	case SEMICOLON:
		emit_token("b(;)");
		if (!incolon) {
			printf(FILE_POSITION "\";\" should only be used inside a colon definition\n",
				iname, lineno);
		} else {
			mark_defined(incolon);
			reset_locals();
		}
		incolon=NULL;
		break;

	case ASSIGN:
		if (!incolon) {
			printf(FILE_POSITION "\"{\" should only be used inside a colon definition\n",
				iname, lineno);
		} else {
			get_word();
			u16 tok = lookup_token((char *)statbuf);
			if (tok == 0xffff) {
				printf(FILE_POSITION "error: no such word '%s' in ->\n",
						iname, lineno, statbuf);
				ERROR;
			}
			u16 typ = lookup_token_type((char *)statbuf);
			if (typ != LOCAL) {
				printf(FILE_POSITION "error: expected word '%s' to be a local in ->\n",
						iname, lineno, statbuf);
				ERROR;
			}
			emit_fcode(0x418 + tok - 0x410);
		}
		break;

	case PARAMS:
		if (!incolon) {
			printf(FILE_POSITION "\"{\" should only be used inside a colon definition\n",
				iname, lineno);
		} else {
			int numparams = 0;
			int numlocals = 0;
			bool onlocals = FALSE;
			while (1) {
				get_word();
				if (!strcmp((char *)statbuf, ";")) {
					if (onlocals) {
						printf(FILE_POSITION "\";\" should only be used once\n",
							iname, lineno);
					} else {
						onlocals = TRUE;
						numparams = numlocals;
					}
				} else if (!strcmp((char *)statbuf, "}")) {
					if (!onlocals) {
						onlocals = TRUE;
						numparams = numlocals;
					}
					if (mac_rom == 1) {
						emit_fcode(0x401);
						emit_byte(numparams);
					} else {
						emit_fcode(0x407 + numparams);
					}
					break;
				} else {
					if (numlocals >= 8) {
						printf(FILE_POSITION "too many locals\n",
							iname, lineno);
					} else if (!strcmp((char *)statbuf, "...")) {
						char buf[20];
						while (numlocals < 8) {
							snprintf(buf, sizeof(buf), "local_%d", numlocals);
							set_local(numlocals++, strdup(buf));
						}
					} else {
						set_local(numlocals++, strdup((char *)statbuf));
					}
				}
			}
		}
		break;

	case CREATE:
		create_word(tok);
		emit_token("b(create)");
		break;

	case DEFER:
		create_word(tok);
		emit_token("b(defer)");
		break;

	case FIELD:
		create_word(tok);
		emit_token("b(field)");
		break;

	case VALUE:
		create_word(tok);
		emit_token("b(value)");
		break;

	case VARIABLE:
		create_word(tok);
		emit_token("b(variable)");
		break;

	case EXTERNAL:
		flags=FLAG_EXTERNAL;
		break;

	case INSTANCE:
		emit_token("instance");
		in_instance=TRUE;
		break;

	case TOKENIZE:
		emit_token("b(')");
		break;

	case AGAIN:
		emit_token("bbranch");
		offs1=dpoptype(kBegin)-(opc-ostart);
		emit_offset(offs1);
		break;

	case ALIAS:
		get_word();
		name=strdup((char *)statbuf);
		get_word();
		alias=strdup((char *)statbuf);
		if(lookup_macro(name))
			printf(FILE_POSITION "warning: duplicate alias\n",
							iname, lineno);
		add_macro(name,alias);
		break;

	case CONTROL:
		get_word();
		emit_num(statbuf[0]&0x1f);
		break;

	case DO:
		emit_token("b(do)");
		dpushtype(kDo, opc-ostart);
		emit_offset(0);
		dpushtype(kDoOffset, opc-ostart);
		break;

	case CDO:
		emit_token("b(?do)");
		dpushtype(kDo, opc-ostart);
		emit_offset(0);
		dpushtype(kDoOffset, opc-ostart);
		break;

	case ELSE:
		offs2=dpoptype(kIf);
		emit_token("bbranch");
		dpushtype(kElse, opc-ostart);
		emit_offset(0);
		emit_token("b(>resolve)");
		offs1=opc-ostart;
		opc=ostart+offs2;
		offs2=offs1-(opc-ostart);
		emit_offset(offs2);
		opc=ostart+offs1;
		break;

	case CASE:
		emit_token("b(case)");
		dpushtype(kCaseEndOf, 0);
		break;

	case ENDCASE:
		/* first emit endcase, then calculate offsets. */
		emit_token("b(endcase)");

		offs1=opc-ostart;

		offs2=dpoptype(kCaseEndOf);
		while (offs2) {
			long tmp;

			opc=ostart+offs2; /* location of offset of endof offset */
			tmp=receive_offset(); /* offset to location */

			if (tmp)
				tmp += (opc-ostart);

#if DEBUG_SCANNER
			printf (FILE_POSITION "debug: endcase endof offset 0x%lx   previous endof offset 0x%lx\n",
				iname,lineno, offs2, tmp);
#endif

			offs2=offs1-(opc-ostart);
			emit_offset(offs2);
			offs2=tmp;
		}
		opc=ostart+offs1;
		break;

	case OF:
		emit_token("b(of)");
		dpushtype(kOf, opc-ostart);
		emit_offset(0);
		break;

	case ENDOF:
		offs1=dpoptype(kOf); /* location of "of" offset */
		emit_token("b(endof)");

		offs2=dpoptype(kCaseEndOf);
		dpushtype(kCaseEndOf, opc-ostart); /* push location of "endof" offset */
		if (offs2 == 0)
			emit_offset(0); /* signify "case" statement */
		else
			emit_offset(offs2-(opc-ostart)); /* emit offset to previous "endof" or "case" offset */

		offs2=opc-ostart; /* location of after offset of "endof" */
		opc=ostart+offs1; /* goto location of "of" offset */
		offs1=offs2-offs1;
		emit_offset(offs1); /* offset of "of" points to after offset of "endof" */

		opc=ostart+offs2; /* location of after offset of "endof" */
		break;

	case HEADERLESS:
		flags=0;
		break;

	case HEADERS:
		flags=FLAG_HEADERS;
		break;

	case DECIMAL:
		/* in a definition this is expanded as macro "10 base !" */
		if (incolon) {
			emit_num(0x0a);
			emit_token("base");
			emit_token("!");
		} else
			base=10;
		break;

	case HEX:
		if (incolon) {
			emit_num(0x10);
			emit_token("base");
			emit_token("!");
		} else
			base=16;
		break;

	case OCTAL:
		if (incolon) {
			emit_num(0x08);
			emit_token("base");
			emit_token("!");
		} else
			base=8;
		break;

	case BINARY:
		if (incolon) {
			emit_num(0x02);
			emit_token("base");
			emit_token("!");
		} else
			base=2;
		break;

	case OFFSET16:
		if (!offs16)
			printf(FILE_POSITION "switching to "
				"16bit offsets.\n", iname, lineno);
		emit_token("offset16");
		offs16=TRUE;
		break;

	case IF:
		emit_token("b?branch");
		dpushtype(kIf, opc-ostart);
		emit_offset(0);
		break;

/* This is a macro.
	case CLEAVE:
*/
	case LEAVE:
		emit_token("b(leave)");
		break;

	case LOOP:
		emit_token("b(loop)");
		offs1=dpoptype(kDoOffset);
		offs2=offs1-(opc-ostart);
		emit_offset(offs2);
		offs1=opc-ostart;
		opc=ostart+dpoptype(kDo);
		offs2=offs1-(opc-ostart);
		emit_offset(offs2);
		opc=ostart+offs1;
		break;

	case CLOOP:
		emit_token("b(+loop)");
		offs1=dpoptype(kDoOffset);
		offs2=offs1-(opc-ostart);
		emit_offset(offs2);
		offs1=opc-ostart;
		opc=ostart+dpoptype(kDo);
		offs2=offs1-(opc-ostart);
		emit_offset(offs2);
		opc=ostart+offs1;
		break;

	case GETTOKEN:
		emit_token("b(')");
		get_word();
		itok=lookup_token((char *)statbuf);
		if (itok==0xffff) {
			printf(FILE_POSITION "error: no such word '%s' in [']\n",
					iname, lineno, statbuf);
			ERROR;
		}
		/* FIXME check size, u16 or token */
		emit_fcode(itok);
		break;

	case ASCII:
		get_word();
		emit_num(statbuf[0]);
		break;

	case CHAR:
		if (incolon)
			printf(FILE_POSITION "warning: CHAR cannot be used inside "
				"of a colon definition.\n", iname, lineno);
		get_word();
		emit_num(statbuf[0]);
		break;

	case CCHAR:
		get_word();
		emit_num(statbuf[0]);
		break;

	case UNTIL:
		emit_token("b?branch");
		emit_offset(dpoptype(kBegin)-(opc-ostart));
		break;

	case WHILE:
		emit_token("b?branch");
		dpushtype(kWhile, opc-ostart);
		emit_offset(0);
		break;

	case REPEAT:
		emit_token("bbranch");
		offs2=dpoptype(kWhile);
		offs1=dpoptype(kBegin)-(opc-ostart);
		emit_offset(offs1);

		emit_token("b(>resolve)");
		offs1=opc-ostart;
		opc=ostart+offs2;
		emit_offset(offs1-offs2);
		opc=ostart+offs1;
		break;

	case THEN:
		emit_token("b(>resolve)");
		offs1=opc-ostart;
		opc=ostart+dpoptypes(kIf, kElse);
		offs2=offs1-(opc-ostart);
		emit_offset(offs2);
		opc=ostart+offs1;
		break;

	case TO:
		emit_token("b(to)");
		in_to=TRUE;
		break;

	case FLOAD:
		{
			u8 *oldstart, *oldpc, *oldend;
			char *oldiname;
			int oldlineno;

			get_word();

			oldstart=start; oldpc=pc; oldend=end;
			oldiname=iname; oldlineno=lineno;

			init_stream((char *)statbuf);
			tokenize();
			close_stream();

			iname=oldiname; lineno=oldlineno;
			end=oldend; pc=oldpc; start=oldstart;
		}
		break;

	case STRING:
		if (*pc++=='\n') lineno++;
		wlen=get_string();
		emit_token("b(\")");
		emit_string(statbuf,wlen-1);
		break;

	case PSTRING:
		if (*pc++=='\n') lineno++;
		wlen=get_string();
		emit_token("b(\")");
		emit_string(statbuf,wlen-1);
		emit_token("type");
		break;

	case PBSTRING:
		if (*pc++=='\n') lineno++;
		get_until(')');
		emit_token("b(\")");
		emit_string(statbuf,strlen((char *)statbuf));
		emit_token("type");
		break;

	case SSTRING:
		if (*pc++=='\n') lineno++;
		get_until('"');
		emit_token("b(\")");
		emit_string(statbuf,strlen((char *)statbuf));
		break;

	case HEXVAL:
		{
			u8 basecpy=base;
			long val;

			base=0x10;
			get_word();
			if (!get_number(&val)) {
				emit_num((u32)val);
			} else {
				printf(FILE_POSITION "warning: illegal value in h#"
						" ignored\n", iname, lineno);
			}
			base=basecpy;
		}
		break;

	case DECVAL:
		{
			u8 basecpy=base;
			long val;

			base=0x0a;
			get_word();
			if (!get_number(&val)) {
				emit_num((u32)val);
			} else {
				printf(FILE_POSITION "warning: illegal value in d#"
						" ignored\n", iname, lineno);
			}

			base=basecpy;
		}
		break;

	case OCTVAL:
		{
			u8 basecpy=base;
			long val;

			base=0x08;
			get_word();
			if (!get_number(&val)) {
				emit_num((u32)val);
			} else {
				printf(FILE_POSITION "warning: illegal value in o#"
						" ignored\n", iname, lineno);
			}

			base=basecpy;
		}
		break;

	case BINVAL:
		{
			u8 basecpy=base;
			long val;

			base=0x02;
			get_word();
			if (!get_number(&val)) {
				emit_num((u32)val);
			} else {
				printf(FILE_POSITION "warning: illegal value in b#"
						" ignored\n", iname, lineno);
			}

			base=basecpy;
		}
		break;

	case BEGINTOK:
		intok=TRUE;
		break;

	case EMITBYTE:
		if (intok)
			emit_byte(dpoptype(kToke));
		else
			printf (FILE_POSITION "warning: emit-byte outside tokenizer"
					" scope\n", iname, lineno);
		break;

	case NEXTFCODE:
		if (intok)
			nextfcode=dpoptype(kToke);
		else
			printf(FILE_POSITION "warning: next-fcode outside tokenizer"
					" scope\n", iname, lineno);
		break;

	case ENDTOK:
		intok=FALSE;
		break;

	case VERSION1:
	case FCODE_V1:
		printf(FILE_POSITION "using version1 header\n", iname, lineno);
		emit_token("version1");
		dpushtype(kFCodeHeader, opc-ostart);
		emit_fcodehdr();
		offs16=FALSE;
		break;

	case START1:
	case FCODE_V2:
	case FCODE_V3: /* Full IEEE 1275 */
		emit_token("start1");
		dpushtype(kFCodeHeader, opc-ostart);
		emit_fcodehdr();
		offs16=TRUE;
		break;

	case START0:
		printf (FILE_POSITION "warning: spread of 0 not supported.",
				iname, lineno);
		emit_token("start0");
		dpushtype(kFCodeHeader, opc-ostart);
		emit_fcodehdr();
		offs16=TRUE;
		break;

	case START2:
		printf (FILE_POSITION "warning: spread of 2 not supported.",
			iname, lineno);
		emit_token("start2");
		dpushtype(kFCodeHeader, opc-ostart);
		emit_fcodehdr();
		offs16=TRUE;
		break;

	case START4:
		printf (FILE_POSITION "warning: spread of 4 not supported.",
			iname, lineno);
		emit_token("start4");
		dpushtype(kFCodeHeader, opc-ostart);
		emit_fcodehdr();
		offs16=TRUE;
		break;

	case FCODE_END:
		haveend=TRUE;
		emit_token("end0");
		finish_fcodehdr();
		break;

	case END0:
		haveend=TRUE;
		emit_token("end0");
		break;

	case END1:
		haveend=TRUE;
		emit_token("end1");
		break;

	case RECURSIVE:
		if (!incolon) {
			printf(FILE_POSITION "\"recursive\" should only be used inside a colon definition\n",
				iname, lineno);
		} else {
			mark_defined(incolon);
		}
		break;

	case RECURSE:
		if (!incolon) {
			printf(FILE_POSITION "\"recurse\" should only be used inside a colon definition\n",
				iname, lineno);
		} else {
			emit_fcode(incolon->fcode);
		}
		break;

	case PCIHDR:
		{
			u32 classid=(u32)dpoptype(kToke);
			u16 did=dpoptype(kToke);
			u16 vid=dpoptype(kToke);

			pci_hdr=opc;
			emit_pcihdr(vid, did, classid);

			printf(FILE_POSITION "PCI header vid=0x%04x, did=0x%04x, classid=0x%06x\n",
				iname, lineno, vid, did, classid);
		}
		break;

	case PCIEND:
		if (!pci_hdr) {
			printf(FILE_POSITION "error: pci-header-end/pci-end "
				"without pci-header\n", iname, lineno);
			ERROR;
		}
		finish_pcihdr();
		break;

	case PCIREV:
		pci_revision=dpoptype(kToke);
		printf(FILE_POSITION "PCI header rev=0x%04x\n",
				iname, lineno, pci_revision);
		break;

	case PCIARCH:
		if (got_pci_entry) {
			printf("error: Only one of pci-architecture and pci-entry is allowed\n");
			ERROR;
		}
		processor_architecture=dpoptype(kToke);
		printf(FILE_POSITION "Processor Architecture=0x%04x\n",
				iname, lineno, processor_architecture);
		got_processor_architecture = TRUE;
		break;

	case PCIENTRY:
		if(got_pci_entry) {
			printf(FILE_POSITION "error: Only one occurrance of pci-entry is allowed\n",
					iname, lineno);
			ERROR;
		}
		if(!pci_hdr) {
			printf(FILE_POSITION "error: pci-entry must follow pci_hdr\n",
					iname, lineno);
			ERROR;
		}
		pci_entry = (u32)(opc-pci_hdr);
		if (pci_entry > 0xFFFF) {
			printf(FILE_POSITION "error: pci-entry must be within 64K of the start of the image\n",
					iname, lineno);
			ERROR;
		}
		printf(FILE_POSITION "Pointer to FCode program=0x%04x\n",
				iname, lineno, pci_entry);
		got_pci_entry = TRUE;
		break;

	case PCIDATASTRUCTURESTART:
		pci_data_structure_start=dpoptype(kToke);
		if(pci_data_structure_start < 0x1A) {
			printf(FILE_POSITION "error: Start of PCI Data Structure=0x%04x is less than 0x1A\n",
					iname, lineno, pci_data_structure_start);
			ERROR;
		}
		printf(FILE_POSITION "Start of PCI Data Structure=0x%04x\n",
				iname, lineno, pci_data_structure_start);
		break;

	case PCIDATASTRUCTURELENGTH:
		pci_data_structure_length=dpoptype(kToke);
		if(pci_data_structure_length < 0x18) {
			printf(FILE_POSITION "error: PCI Data Structure length=0x%04x is less than 0x18\n",
					iname, lineno, pci_data_structure_length);
			ERROR;
		}
		printf(FILE_POSITION "PCI Data Structure length=0x%04x\n",
				iname, lineno, pci_data_structure_length);
		break;

	case VPDOFFSET:
		pci_vpd=dpoptype(kToke);
		printf(FILE_POSITION "Vital Product Data offset=0x%04x\n",
				iname, lineno, pci_vpd);
		break;

	case ROMSIZE:
		rom_size=(u32)dpoptype(kToke);
		printf(FILE_POSITION "Rom Size=0x%08x\n",
				iname, lineno, rom_size);
		break;

	case NOTLAST:
		pci_is_last_image=FALSE;
		printf(FILE_POSITION "PCI header not last image!\n",
				iname, lineno);
		break;

	case ABORTTXT:
		/* ABORT" is not to be used in FCODE drivers
		 * but Apple drivers do use it. Therefore we
		 * allow it. We push the specified string to
		 * the stack, do -2 THROW and hope that THROW
		 * will correctly unwind the stack.
		 */

		printf(FILE_POSITION "warning: ABORT\" in fcode not defined by "
				"IEEE 1275-1994.\n", iname, lineno);
		if (*pc++=='\n') lineno++;
		wlen=get_string();

#ifdef BREAK_COMPLIANCE
		/* IF */
		emit_token("b?branch");

		dpushtype(kAbort, opc-ostart);
		emit_offset(0);
#endif
		emit_token("b(\")");
		emit_string(statbuf,wlen-1);
#ifdef BREAK_COMPLIANCE
		emit_token("type");
#endif
		emit_token("-2");
		emit_token("throw");
#ifdef BREAK_COMPLIANCE
		/* THEN */
		emit_token("b(>resolve)");
		offs1=opc-ostart;
		opc=ostart+dpoptype(kAbort);
		offs2=offs1-(opc-ostart);
		emit_offset(offs2);
		opc=ostart+offs1;
#endif
		break;

	case FCODE_DATE:
		{
			time_t tt;
			char fcode_date[11];

			tt=time(NULL);
			strftime(fcode_date, 11, "%m/%d.%Y",
					localtime(&tt));
			emit_token("b(\")");
			emit_string((u8 *)fcode_date,10);
		}
		break;

	case FCODE_TIME:
		{
			time_t tt;
			char fcode_time[9];

			tt=time(NULL);
			strftime(fcode_time, 9, "%H:%M:%S",
					localtime(&tt));
			emit_token("b(\")");
			emit_string((u8 *)fcode_time,8);
		}
		break;

	case ENCODEFILE:
		get_word();
		encode_file( (char*)statbuf );
		break;

	default:
		printf(FILE_POSITION "error: Unimplemented control word '%s'\n",
				iname, lineno, statbuf);
		ERROR;
	}
}

void tokenize(void)
{
#define __debugcomparefile 0

	unsigned long wlen;
	u16 tok;
	char *mac;
	long val;

#if __debugcomparefile
	u8 *copyoffile = malloc(end-start);
	if (copyoffile)
		memcpy(copyoffile, start, end-start);
#endif


	while (1)
	{

#if __debugcomparefile
		if (memcmp(copyoffile, start, end-start))
		{
			printf( "lineno %d\n", lineno );
			memcpy(copyoffile, start, end-start);
		}
#endif

		if ((wlen=get_word())==0)
			break;

		/* Filter comments */
		switch (statbuf[0]) {
		case '\\':
			pc-=wlen;
			get_until('\n');
			continue;

		case '(':
			/* only a ws encapsulated '(' is a stack comment */
			if (statbuf[1])
				break;

			pc-=wlen;
			get_until(')');
#if DEBUG_SCANNER
			printf (FILE_POSITION "debug: stack diagram: %s)\n",
						iname, lineno, (char *)statbuf);
#endif
			continue;
		}

		/* Check whether it's a non-fcode forth construct */

		tok=lookup_fword((char *)statbuf);
		if (tok!=0xffff) {
#if DEBUG_SCANNER
			printf(FILE_POSITION "debug: matched internal opcode 0x%04x\n",
					iname, lineno, tok);
#endif
			validate_to_target(0);
			validate_instance(tok);

			handle_internal(tok);

#if DEBUG_SCANNER
			if (verbose)
				printf(FILE_POSITION "debug: '%s' done.\n",
					iname, lineno, (char *)statbuf);
#endif
			continue;
		}

		/* Check whether it's one of the defined fcode words */

		tok=lookup_token((char *)statbuf);
		if (tok!=0xffff) {
#if DEBUG_SCANNER
			printf(FILE_POSITION "debug: matched fcode no 0x%04x\n",
					iname, lineno, tok);
#endif
			validate_to_target(lookup_token_type((char *)statbuf));
			validate_instance(0);

			emit_fcode(tok);
			continue;
		}

		/* Check whether a macro with given name exists */

		mac=lookup_macro((char *)statbuf);
		if(mac) {
			u8 *oldstart, *oldpc, *oldend;
#if DEBUG_SCANNER
			printf(FILE_POSITION "debug: macro %s folds out to sequence"
				" '%s'\n", iname, lineno, (char *)statbuf, mac);
#endif
			validate_to_target(0);
			validate_instance(0);

			oldstart=start; oldpc=pc; oldend=end;
			start=pc=end=(u8*)mac;
			end+=strlen(mac);

			tokenize();

			end=oldend; pc=oldpc; start=oldstart;

			continue;
		}

		/* It's not a word or macro - is it a number? */

		if (!get_number(&val)) {
			if (intok)
				dpushtype(kToke, val);
			else {
				validate_to_target(0);
				validate_instance(0);
				emit_num((u32)val);
			}
			continue;
		}

		/* could not identify - pretend it exists instead of bailing out, but report it to the user. */

		printf(FILE_POSITION "error: word '%s' is not in dictionary.\n",
				iname, lineno, (char *)statbuf);

		char* name=strdup((char *)statbuf);
		add_token(nextfcode, name);

		validate_to_target(0);
		validate_instance(0);

		emit_fcode(nextfcode);
		nextfcode++;
		gGotError = 1;
		/* ERROR */

	} /* while */

	validate_to_target(0);
	validate_instance(0);
}
