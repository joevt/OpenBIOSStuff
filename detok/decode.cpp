/*
 *                     OpenBIOS - free your system! 
 *                            ( detokenizer )
 *                          
 *  decode.c - contains output wrappers for fcode words.  
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
#include <ctype.h>
#include <errno.h>
#include <string.h>

#include "detok.h"
#include "stream.h"
#include "dictionary.h"
#include "decode.h"
#include "stack.h"
#include "macrom.h"

u32 current_token_pos;
u16 current_token_fcode;
token_t* current_token_record;
const char * current_token_name;

int gPass;

int indent=0, use_tabs=0, verbose=0, decode_all=0, ignore_len=0, mac_rom=0, linenumbers=0;
u32 romstartoffset = 0;

bool offs16;
bool got_start;
bool isSpecialStart;

int linenum;
u32 fclen;
size_t filelen;
u32 gStartPos;
u32 gLastPos;
u32 gLastFcodeImageEndPos;
int gLastMacRomTokenPos;


void dump_line(u32 pos)
{
	linenum++;
	if ( gPass )
	{
		if (linenumbers & 1)
			printf("%6d: ",linenum);

		if (linenumbers & 2)
			printf("%06X: ",pos + romstartoffset);
	}
}


static int myisprint(int c)
{
	return ((c>=' ' && c<='~') || c=='\t');
}


void pretty_string(char *string, u8 len, char* strbuff, size_t sizebuff)
{
	char* curbuff = strbuff;
	size_t inc = 0; 

#if 0
	u8 c;
	bool qopen=TRUE;
	
	curbuff += sprintf(curbuff, "\"");

	while ((c=(*(string++)))) {
		if (isprint(c)) {
			if (!qopen) {
				curbuff += sprintf(curbuff, "\"");
				qopen=TRUE;
			}
			curbuff += sprintf(curbuff, "%c",c);
		} else {
			if (qopen) {
				curbuff += sprintf(curbuff, "\"");
				qopen=FALSE;
			}
			curbuff += sprintf(curbuff, " 0x%02x",c);
		}
	}
	if (qopen)
		curbuff += sprintf(curbuff, "\"");
#else
	u8 c;
	bool qopen=TRUE;

	u8 * cptr;
	int numleft;
	bool hexonly = FALSE;
	int ndx = 0;
	
	cptr = (u8 *)string;
	for ( numleft = len; numleft > 0; numleft-- )
	{
		c=(*(cptr++));
		if ( ( c == 0 && len >= 252 ) || ( c != 0 && myisprint(c) == FALSE ) )
		{
			hexonly = TRUE;
			break;
		}
	}

	inc += scnprintf(curbuff+inc, sizebuff-inc, "\" ");

	cptr = (u8 *)string;
	for ( numleft = len; numleft > 0; numleft-- )
	{
		c=(*(cptr++));
		if (hexonly == FALSE && c == '"') {
			if (!qopen) {
				inc += scnprintf(curbuff+inc, sizebuff-inc, ")\"\"");
				qopen=TRUE;
			}
			else
				inc += scnprintf(curbuff+inc, sizebuff-inc, "\"\"");
		} else if (hexonly == FALSE && myisprint(c)) {
			if (!qopen) {
				inc += scnprintf(curbuff+inc, sizebuff-inc, ")%c",c);
				qopen=TRUE;
			}
			else
				inc += scnprintf(curbuff+inc, sizebuff-inc, "%c",c);
		} else {
			if (qopen) {
				inc += scnprintf(curbuff+inc, sizebuff-inc, "\"(%02X",c);
				qopen=FALSE;
			}
			else
				if ( ndx % 2 )
					inc += scnprintf(curbuff+inc, sizebuff-inc, "%02X",c);
				else
					inc += scnprintf(curbuff+inc, sizebuff-inc, " %02X",c);
		}
		ndx++;
	}
	if (!qopen) {
		inc += scnprintf(curbuff+inc, sizebuff-inc, ")");
		qopen=TRUE;
	}

	if ( hexonly )
	{
		char buffer[256];
		make_buffer_text( buffer, (u8*)string, len );
		inc += scnprintf(curbuff+inc, sizebuff-inc, "\" \\ \"%s\" ", buffer );
	}
	else
		inc += scnprintf(curbuff+inc, sizebuff-inc, "\" ");
#endif
}


static void decode_indent(void) 
{
	if ( gPass )
	{
		int i;

		if (indent<0) {
			fprintf(stderr, "Line %d # Indentation negative error\n", linenum);
			indent=0;
		}

		if (indent>1000) {
			fprintf(stderr, "Line %d # Indentation too big error\n", linenum);
			indent=1000;
		}

		if (use_tabs)
			for (i=0; i<indent; i++)
				printf ("\t");
		else
			for (i=0; i<indent; i++)
				printf ("    ");
	}
}


void decode_lines(void)
{
	u32					endPos = (u32)get_streampos();
	u32					currentPos = gLastPos;

	if ( currentPos > endPos )
		if ( !gPass )
			fprintf( stderr, "Line %d # currentPos %X > endPos %X)\n", linenum + 1, currentPos, endPos );

	set_streampos( currentPos );

	disassemble_lines(endPos, currentPos);

	dump_line(endPos);
}


static void output_token_of(bool of)
{
	if ( gPass )
	{
		const char * tname = NULL;
		token_t *theToken = find_token( fcode );
		if (theToken) {
			if (of) tname = theToken->ofname;
			if (!tname) tname = theToken->name;
		}
		if (!tname) tname = fcerror;

		if (tname == unnamed)
			printf( "unnamed_fcode_%03x ", fcode );
		else
			printf ("%s ", tname);

		/* The fcode number is interesting if
		 *  (a) detok is in verbose mode
		 *  (b) the token has no name.
		 */
			
		if (verbose)
			printf("\\ [0x%03x] ", fcode);

		if (tname == fcerror && indent > 20)
			indent=0;
	}
}


void output_token(void)
{
	output_token_of(FALSE);
}


static s32 decode_offset(bool islong)
{
	s32 offs;

	u32 offsetaddr = (u32)get_streampos();

	offs=get_offset(islong);

	if (!islong && offs == 0)
	{
		if ( gPass )
			fprintf(stderr, "Line %d # Offset 0, switching to 16 bit offsets\n", linenum);
		set_streampos( offsetaddr );
		offs16 = TRUE;
		offs=get_offset(islong);
	}

	if ( gPass )
	{
		output_token();
		if (islong && offs > 0x7fff)
			printf("0x0%x\n",offs);
		else
			printf("0x%x\n",offs);
	}
	return offs;
}


void decode_default_of(bool of)
{
	if ( gPass )
	{
		output_token_of(of);
		printf ("\n");
	}
}


static void decode_default(void)
{
	decode_default_of(FALSE);
}


static void new_token(void)
{
	u16 token;
	output_token();
	token=get_token();
	if ( gPass )
		printf("0x%03x\n",token);
	add_token(token, unnamed);
}


bool good_tokenname(char* tokenname, u8 len)
{
	size_t pos;
	return (pos=strspn(tokenname, "!\"#$%&'()*+,-./0123456789:;<=>?@[\\]^_`abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ{|}~")) && (tokenname[pos]==0) && (pos==len);
}


static void named_token(bool isExternal)
{
	#pragma unused(isExternal)
	u16 token;
	char* string;
	u32 pos;
	u8 len;

	output_token();

	pos=get_streampos();
	string=get_string(&len);
	token=get_token();

	if (good_tokenname(string, len))
	{
		if ( gPass )
			printf("%s 0x%03x\n", string, token);
		add_token(token,string);
	}
	else
	{
		char* p;
		int numleft;

		set_streampos(pos);

		if ( gPass )
		{
			numleft = len;
			for (p=string; numleft; numleft--,p++)
				if (*p==13 || *p==10 || *p==0)
					*p = ' ';

			printf("%s 0x%03x \\ bad name, ignored\n", string, token);
			fprintf(stderr, "Line %d # Bad token name: \"%s\"\n", linenum, string);
		}

		free(string);
	}
}


static void bquote(void)
{
	char *string;
	u8 len;
	char strbuff[2000];

	string=get_string(&len);
	if ( gPass )
	{
		output_token();
		pretty_string(string, len, strbuff, sizeof(strbuff));
		printf("%s\n", strbuff);
	}
	free(string);
}


static void blit(void)
{
	u32 lit;

	lit=get_num32();
	if ( gPass )
	{
		output_token();
		printf("0x%x\n",lit);
	}
}


static void offset16(void)
{
	decode_default();
	offs16=TRUE;
}


typedef struct vtoken {
	u16 fcode;
	u32 pos;
	struct vtoken *next;
} vtoken_t;

vtoken_t *vtokens=NULL;


vtoken_t *add_virtual_token(u16 number, u32 pos) {
	vtoken_t *curr = (vtoken_t*)malloc(sizeof(vtoken_t));
	if(!curr) {
		printf("Out of memory while adding virtual token.\n");
		exit(-ENOMEM);
	}
	curr->next = vtokens;
	curr->fcode = number;
	curr->pos = pos;
	vtokens = curr;
	return curr;
}


static void decode_branch(bool islong)
{
	u32 offsetaddr = (u32)get_streampos();
	s32 offs = decode_offset(islong);
	if (offs>=0) {
		indent++;
		if (islong && !gPass) add_virtual_token(0x0b2, offs + offsetaddr); // b(>resolve)
	}
	else {
		indent--;
		if (islong && !gPass) {
			add_virtual_token(0x0b1, offs + offsetaddr); // b(<mark)
			// Actually, the following can't be true since long branch doesn't support negative offsets!
			fprintf(stderr, "Line %d # long branch with negative offset means line numbers are not accurate for first pass.\n", linenum);
			linenum++;
		}
	}
}


static void decode_two(void)
{
	output_token();
	get_token();
	decode_default();
}


static void decode_start(void)
{
	u8  fcformat;
	u16 fcchecksum, checksum=0;
	u32 newLen;
	u32 newEndPos=0;
	u32 numCheckSumBytes=0;
	u32 i;
	bool problem=TRUE;
	bool had_start=got_start;

	/* current_token_pos */			/* 0 */
	fcformat=get_num8();			/* 1 */
	fcchecksum=get_num16();			/* 2 */
	newLen=get_num32();				/* 4 */
									/* 8 */

	// newLen might be invalid if this is not actually a start token.
	// Therefore we must ensure the comparison doesn't overflow.
	// This is why current_token_pos is subtracted from 0x7fffffff
	// instead of added to newLen.
	if ( newLen > 8 && newLen <= 0x7fffffff - current_token_pos )
	{
		newEndPos = newLen + current_token_pos;
		// if (newEndPos >= newLen) /* no overflow */
		if (newEndPos <= filelen)
		{
			numCheckSumBytes = newLen - 8;
			
			for (i=0; i < numCheckSumBytes; i++)
				checksum+=get_num8();

			if (current_token_pos==0 || fcchecksum==checksum)
			{
				fclen=newEndPos;
				indent=0;
				problem=FALSE;

				offs16 = current_token_fcode != 0x0fd; /* version1 */
				gStartPos=current_token_pos;
				got_start = TRUE;
			}
		}
	}

	if (got_start)
	{
		isSpecialStart = FALSE;
		
		if (!had_start)
		{
			set_streampos(current_token_pos);
			decode_lines();
			if ( gPass )
				printf ("\\ detokenizing start at offset %06X\n", current_token_pos + romstartoffset);
			dump_line( current_token_pos );
		}

		if ( gPass )
/*
		if (fcchecksum==checksum)
*/
			if (current_token_pos>0)
				fprintf(stderr, "Line %d # Start\n", linenum);

		decode_indent();
		decode_default();
		set_streampos(current_token_pos+1);

		dump_line(get_streampos());
		decode_indent();
		get_num8();
		if ( gPass )
			printf("  format:    0x%02x\n", fcformat);

		dump_line(get_streampos());
		decode_indent();
		get_num16();
		if ( gPass )
			printf("  checksum:  0x%04x (%s)\n", fcchecksum, numCheckSumBytes>0? (fcchecksum==checksum? "ok":"not ok"):"none");

		dump_line(get_streampos());
		decode_indent();
		get_num32();
		if ( gPass ) {
			if (newEndPos<=filelen)
				printf("  len:       0x%x (%u bytes)\n", newLen, newLen);
			else
				printf("  len:       0x%x (%u bytes) is greater than file length 0x%lx (%ld bytes)\n", newLen, newLen, filelen-current_token_pos, filelen-current_token_pos);
		}

		if (problem)
			set_streampos(current_token_pos+1);

		mark_last_streampos();
	}
	else 
		set_streampos(current_token_pos+1);
}


static void decode_token(u16 token_to_decode)
{
/*
	if ( get_streampos() >= 0x01F714 )
		DebugStr( "\phello" );
*/
	switch (token_to_decode) {
		case 0x0f0: /* start0 */
		case 0x0f1: /* start1 */
		case 0x0f2: /* start2 */
		case 0x0f3: /* start4 */
		case 0x0fd: /* version1 */
			decode_start();
			break;

		default:
		{
			if (got_start)
			{
				switch (token_to_decode)
				{
					case 0x0c2: /* b(;) */
					case 0x0c5: /* b(endcase) */
						{
							int theindent = (int)dpoptype(token_to_decode == 0xc2 ? 0x0b7 : 0x0c5);
							if (indent != theindent) {
								fprintf(stderr, "Line %d # indent is %d but expected %d for token 0x%x\n", linenum, indent, theindent, token_to_decode);
								indent = theindent;
							}
						}
						break;
				}
				decode_indent();
				switch (token_to_decode)
				{
					case 0x000: /* end0 */
					case 0x0ff: /* end1 */
						if ( isSpecialStart )
						{
							got_start = FALSE;
							isSpecialStart = FALSE;
							if (token_to_decode == 0x000) {
								decode_default();
								dump_line(get_streampos());
							}
							else {
								set_streampos(current_token_pos);
							}
							if ( gPass )
								printf ("\\ detokenizing finished after %u bytes.\n",
										get_streampos() - gStartPos );
							gLastFcodeImageEndPos = get_streampos();
						}
						else {
							decode_default();
						}
						break;
					case 0x0b5: /* new-token */
						new_token();
						break;
					case 0x0b6: /* named-token */
						named_token(FALSE);
						break;
					case 0x0ca: /* external-token */
						named_token(TRUE);
						break;
					case 0x012: /* b(") */
						bquote();
						break;
					case 0x010: /* b(lit) */
						blit();
						break;
					case 0x0cc: /* offset16 */
						offset16();
						break;
					case 0x013: /* bbranch */
					case 0x014: /* b?branch */
						decode_branch(false);
						break;
					case 0x3fe: /* lb?branch */
					case 0x3ff: /* lbbranch */
						if (mac_rom) {
							decode_branch(true);
						}
						else {
							decode_default();
						}
						break;
					case 0x0b1: /* b(<mark) */
						decode_default();
						indent++;
						break;
					case 0x0b7: /* b(:) */
					case 0x0c4: /* b(case) */
						decode_default();
						indent++;
						dpushtype(token_to_decode, indent);
						break;
					case 0x0c2: /* b(;) */
					case 0x0c5: /* b(endcase) */
					case 0x0b2: /* b(>resolve) */
						decode_default();
						indent--;
						break;
					case 0x015: /* b(loop) */
					case 0x016: /* b(+loop) */
					case 0x0c6: /* b(endof) */
						decode_offset(false);
						indent--;
						break;
					case 0x017: /* b(do) */
					case 0x018: /* b/?do) */
					case 0x01c: /* b(of) */
						decode_offset(false);
						indent++;
						break;
					case 0x011: /* b(') */
					case 0x0c3: /* b(to) */
						decode_two();
						break;

/* Power Mac OF 1.0.5 ROM tokens */

	/*

	Forth:			{ local1, local2, ..., localn ; localn + 1, ... } \ ";" and local names are optional

	Tokenized:		0x401 n					\\ n is a byte (PMac OF 1.0.5 Mac rom max is 5)

	Compiled:		li r3,n
					bl mac_rom_code_45C

	Detokenized:	byte ^-7F61F8

	*/
					case 0x401:
						if (mac_rom & 0x101)
							local_variables_declaration();
						else
							goto thedefault;
						break;
					
					case 0x0f8:
						if (mac_rom & 4)
							code_s();
						else
							goto thedefault;
						break;

					default:
thedefault:
						decode_default_of(TRUE);
				} /* switch token_to_decode */
				mark_last_streampos();
			}
			else
			{
				switch (token_to_decode) {
					case 0x0bf: /* b(code) */
					case 0x0b7: /* b(:) */
					case 0x0b8: /* b(value) */
					case 0x0b9: /* b(variable) */
					case 0x0ba: /* b(constant) */
					case 0x0bb: /* b(create) */ /* •••••••••• */
					case 0x0bc: /* b(defer) */
					case 0x0bd: /* b(buffer:) */
					case 0x0be: /* b(field) */
					case 0x0db: /* set-token */
						decode_rom_token();
						break;
					case 0x010:
						if ( mac_rom & 2 ) {
							// for doing bytes between rom images of Open Firmware 2
							decode_rom_token2();
						}
						break;
					case 0x000:
						if ( mac_rom & 0x200 ) {
							// for doing bytes between rom images of Open Firmware 2.0.3
							decode_rom_token203();
						}
						if ( mac_rom & ~0x303 ) {
							// for doing bytes between rom images in Open Firmware 2.0.3 and Open Firmware 3 and later
							decode_rom_token3();
						}
						break;
				}
			}
		} /* default */
	} /* switch */

	if ( got_start && current_token_record )
	if ( gPass )
	{
		if ( (current_token_record->flags & kFCodeTypeMask) == kFCodeHistorical )
			fprintf(stderr, "Line %d # Historical or non-implemented FCode: \"%s\" [0x%03x]\n", linenum, lookup_token(token_to_decode), token_to_decode);

		if ( (current_token_record->flags & kFCodeTypeMask) == kFCodeNew )
			fprintf(stderr, "Line %d # New FCode: \"%s\" [0x%03x]\n", linenum, lookup_token(token_to_decode), token_to_decode);
	}
}


u32 currvtokenpos = 0;
vtoken_t *currvtoken = NULL;

u16 get_a_token() {
	if (currvtokenpos != current_token_pos ) {
		currvtoken = vtokens;
		currvtokenpos = current_token_pos;
	}
	
	while (currvtoken) {
		if (currvtoken->pos == current_token_pos) {
			fcode = currvtoken->fcode;
			currvtoken = currvtoken->next;
			return fcode;
		}
		currvtoken = currvtoken->next;
	}
	
	return get_token();
}


int detokenize(void)
{
	for ( gPass = 0; gPass <= 1; gPass++ )
	{
		fprintf(stderr, "Doing Pass %d\n", gPass + 1);
		
		offs16=TRUE;
		isSpecialStart=FALSE;
		gStartPos=0;
		gLastPos=0;
		gLastFcodeImageEndPos = 0;
		gLastMacRomTokenPos=-1;
		linenum=0;
		currvtokenpos = 0;
		currvtoken = NULL;
		
		set_streampos(0);

		do {
			got_start = !(mac_rom != 0);

			do {
				if (got_start)
					decode_lines();

				current_token_pos = get_streampos();

				current_token_fcode = get_a_token();
				current_token_record = find_token( current_token_fcode );
				current_token_name = get_token_name( current_token_record );
				decode_token(current_token_fcode);

			} while ( (current_token_fcode||decode_all) && (get_streampos() < fclen) );

			decode_lines();

			if (gStartPos > 0)
			{
				if ( gPass )
				{
					printf ("\\ detokenizing finished after %u of %u bytes.\n", get_streampos() - gStartPos, fclen - gStartPos );
					fprintf(stderr, "Line %d # End\n", linenum);
				}
				gStartPos=0;
			}
			else
			{
				if ( gPass )
					printf ("\\ detokenizing finished after %u of %u bytes.\n", get_streampos(), fclen );
			}
			gLastFcodeImageEndPos = get_streampos();

			fclen = (u32)filelen;
			
		} while (ignore_len && (get_streampos() < filelen) );
	}

	if (mac_rom)
		dump_defined();

	return 0;
}
