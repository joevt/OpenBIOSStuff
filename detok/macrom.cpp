/*
 *
 *	macrom.cpp
 *	
 *	joevt May 11, 2023
 *
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

#ifdef USEDISASSEMBLER
	#ifdef USECAPSTONE
		#include <capstone/capstone.h>
	#else
		#include "Disassembler.h"
	#endif
#endif


bool debugcapstone = 0;

bool gLastTokenIsAlias = FALSE;


int scnprintf(char * str, size_t size, const char * format, ...) {
	va_list args;
	va_start(args, format);
	size_t result = vsnprintf(str, size, format, args);
	va_end(args);
	if (result > size)
		result = size;
	return (int)result;
}


static token_t *find_token_by_pos(u32 pos, u32* offset)
{
	token_t*	curr;
	s32			currOffset;
	token_t*	closestToken = NULL;
	s32			closestOffset = 0x7FFFFFFF;
	
	if (pos < filelen) {
		for ( curr = dictionary; curr != NULL; curr = curr -> next )
		{
			if ( curr->execution_pos >= 0 )
			{
				currOffset = pos - curr->execution_pos;

				if ( currOffset == 0 )
				{
					*offset = currOffset;
					return curr;
				}
			
				if ( currOffset >= 0 && ( closestToken == NULL || currOffset < closestOffset ) )
				{
					closestToken = curr;
					closestOffset = currOffset;
				}
			}
		}
	}

	*offset = closestOffset;
	return closestToken;
}


static token_t *find_token_by_hlinkpos(u32 pos, u32* offset)
{
	token_t*	curr;
	s32			currOffset;
	token_t*	closestToken = NULL;
	s32			closestOffset = 0x7FFFFFFF;
	
	for ( curr = dictionary; curr != NULL; curr = curr -> next )
	{
		if ( curr->hlink_pos >= 0 )
		{
			currOffset = pos - curr->hlink_pos;

			if ( currOffset == 0 )
			{
				*offset = currOffset;
				return curr;
			}
			
			if ( currOffset >= 0 && ( closestToken == NULL || currOffset < closestOffset ) )
			{
				closestToken = curr;
				closestOffset = currOffset;
			}
		}
	}

	*offset = closestOffset;
	return closestToken;
}


static token_t *find_next_token_by_pos(u32 pos)
{
	token_t*	curr;
	token_t*	closestToken = NULL;
	s32			closest_pos = 0x7FFFFFFF;
	
	for ( curr = dictionary; curr != NULL; curr = curr -> next )
	{
		if ( curr->execution_pos >= 0 && curr->execution_pos > pos && curr->execution_pos < closest_pos )
		{
			closest_pos = curr->execution_pos;
			closestToken = curr;
		}
	}
	return closestToken;
}


static u8* storebe(u32 num, u8* bytes)
{
		bytes[0] = num >> 24;
		bytes[1] = num >> 16;
		bytes[2] = num >> 8;
		bytes[3] = num;
		return bytes;
}


static void dump_buffer_hex( u8 *buffer, int len, int spaceBytes )
{
	int					index;

	for ( index = 1; index <= len; index++ )
	{
		if (index % spaceBytes == 0)
			printf( "%02X ", *(u8*)buffer++ );
		else
			printf( "%02X", *(u8*)buffer++ );
	}
}


void make_buffer_text( char *dstBuffer, u8 *srcBuffer, int len )
{
	u8* dstCurrent = (u8*)dstBuffer;
	
	while ( len-- > 0 )
	{
		u8 c = *(u8*)srcBuffer++;
		if ( c < ' ' || c > 0xFE || c == 0x7F )
			c = '.';
		*dstCurrent++ = c;
	}
	*dstCurrent++ = 0;
}


static void dump_hex_one_line( u8* buffer, int len )
{
	if ( gPass )
	{
		char textBuffer[33];

		if ( len > 32 )
		{
			len = 32;
			fprintf(stderr, "Line %d # Too many hex bytes to dump\n", linenum);
		}

		dump_buffer_hex( buffer, len, 2 );
		make_buffer_text( textBuffer, buffer, len );

		printf( "%*s \"%s\"\n", ((32 - len) * 2 * 5 + 2) / 4, "", textBuffer );
	}
}


static void dumptokenoffset( s32 previous_token_offset, u32 previous_token_address )
{
	if ( gPass )
	{
		char comment[10];
		char instructionText[5];

		u8 bytes[4];
		dump_buffer_hex( storebe( previous_token_offset, bytes ), 4, 2 );
		make_buffer_text( instructionText, bytes, 4 );
		snprintf( comment, sizeof(comment), "%08X", previous_token_address );
		printf( "%12s          %-8s %-39s \"%s\"\n", comment, "", "", instructionText );
	}
}


static void dump_hex( u32 currentPos, u32 endPos )
{
	u8					buffer[32];
	u8 *				bufferCurrent;
	int					len;
	int					maxlen;

	maxlen = 4 - (currentPos & 3);
	if ( maxlen == 4 )
		maxlen = 32;
	
	if ( !gPass )
	{
		u32 total = endPos - currentPos;
		if ( total < maxlen )
			maxlen = total;
		total = total - maxlen;
		linenum += ( total + 31 ) / 32 + ( maxlen > 0 );
		set_streampos( endPos );
	}
	else
	{
		set_streampos( currentPos );
		
		while (currentPos < endPos)
		{
			dump_line(currentPos);
			
			len = 0;
			bufferCurrent = buffer;
			while ( (currentPos < endPos) && (len < maxlen) )
			{
				*bufferCurrent++ = get_num8();
				len++;
				currentPos++;
			}

			dump_hex_one_line( buffer, len );
			maxlen = 32;
		}
	}
}


#ifdef USEDISASSEMBLER

u32					instruction;
u32					instructionMem;
u32					gSpecialPurposeRegister;
token_t*			gDissasembledToken = NULL;
u32                 gDissasembledTokenOffset = 0;

#ifdef USECAPSTONE

csh handle = 0;

bool cs_check(void) {
	if (!handle) {
		if (cs_open( CS_ARCH_PPC, (cs_mode)(CS_MODE_32 | CS_MODE_64 | CS_MODE_BOOKE | CS_MODE_BIG_ENDIAN), &handle ) == CS_ERR_OK) {
			cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
		}
	}
	return handle != 0;
}

#else

DisassemblerOptions	options = Disassemble_PowerPC32 | Disassemble_Extended | Disassemble_DecField | Disassemble_DecOffset | Disassemble_DecPCRel | Disassemble_CRBits | Disassemble_BranchBO | Disassemble_TrapTO;


static char * EmptyDisassemblerLookups(
				void*                           refCon,
				const unsigned long *           cia, 
				const DisassemblerLookupType    lookupType,
				const DisLookupValue            thingToReplace)
{
	#pragma unused( refCon, cia, thingToReplace )
	switch ( lookupType )
	{
		case Disassembler_Lookup_SPRegister:
			#warning "I don't think this works with HID1; please test instructions 7E91FAA6 and 7E91FBA6"
			return NULL;	/* the default lookup will detect unknown special purpose registers */
		default:
			return "_";
	}
}


static char * MyDisassemblerLookups(
				void*							refCon,
				const unsigned long *			cia, 
				const DisassemblerLookupType	lookupType,
				const DisLookupValue            thingToReplace)
{
	#pragma unused( refCon, cia )
	static char substitute_string[100];
	
	gDissasembledToken = NULL;

	switch ( lookupType )
	{
		case Disassembler_Lookup_GPRegister:
		{
			if ( thingToReplace.gpr == 2 )
				return "r2";
			if ( thingToReplace.gpr == 1 )
				return "r1";
			break;
		}
		
		case Disassembler_Lookup_RelAddress:
		{
			istoken = thingToReplace.relAddress - romstartoffset <= gLastPos;
			if (!istoken) {
				token_t* nextToken = find_next_token_by_pos( gLastPos );
				istoken = nextToken && ( imm - romstartoffset >= nextToken->execution_pos );
			}

			if ( !istoken )
				break;

			gDissasembledToken = find_token_by_pos( thingToReplace.relAddress - romstartoffset, &gDissasembledTokenOffset );
		
			if ( gDissasembledToken == NULL )
				break;

			if ( gDissasembledTokenOffset == 0 )
				return gDissasembledToken->name;
				
			snprintf( substitute_string, sizeof(substitute_string), "%s%+ld", gDissasembledToken->name, gDissasembledTokenOffset );
			{
				gDissasembledToken = NULL;
				return substitute_string;
			}

			break;
		}
		
		case Disassembler_Lookup_SImmediate:
		{
			if ( (instruction & 0xFC1F0000) == 0x3C000000 ) /* lis */
			{
				if ( options & Disassemble_DollarHex )
					snprintf( substitute_string, sizeof(substitute_string), "$%04X", thingToReplace.si & 0x0FFFF );
				else
					snprintf( substitute_string, sizeof(substitute_string), "0x%04X", thingToReplace.si & 0x0FFFF );
				return substitute_string;
			}
			else
			{
				u32 num = abs( thingToReplace.si );
				
				if
					(
							( num < 10 )
						||	( num % 10 == 0 && num <= 100 )
						||	( num % 100 == 0 && num <= 10000 )
						||	( num % 1000 == 0 )
					)
				{
					snprintf( substitute_string, sizeof(substitute_string), "%d", thingToReplace.si );
					return substitute_string;
				}
			}
			break;
		}

		case Disassembler_Lookup_UImmediate:
		{
				u32 num = thingToReplace.ui;
				if
					(
							( num < 10 )
						||	( num % 10 == 0 && num <= 100 )
						||	( num % 100 == 0 && num <= 10000 )
						||	( num % 1000 == 0 )
					)
				{
					snprintf( substitute_string, sizeof(substitute_string), "%u", thingToReplace.ui );
					return substitute_string;
				}
				break;
		}
		
		case Disassembler_Lookup_SPRegister:
		{
			gSpecialPurposeRegister = thingToReplace.spr;

			switch( thingToReplace.spr )
			{
				/* G3 (MPC750) registers - don’t exist on the G5 */
				case 1017:
					return "L2CR"; /* L2 Cache Control Register (L2CR) */
				case 1019:
					return "ICTC"; /* Instruction Cache Throttling Control Register (ICTC) */
				
				
				default:
					if ( mac_rom & 4 )
						/* G5 (PPC 970fx) registers */
						switch( thingToReplace.spr )
						{
							case 276:
								return "SCOMC"; /* Scan Communications Register (SCOMC) */
							case 277:
								return "SCOMD"; /* Scan Communications Register (SCOMD) */
							case 311:
								return "HIOR"; /* Hardware Interrupt Offset Register (HIOR) */
							case 1012:
								return "HID4"; /* Hardware Implementation Register (HID4) */
							case 1014:
								return "HID5"; /* Hardware Implementation Registers (HID5) */
							case 1015:
								return "DABRX"; /* Data Address Breakpoint Register (DABRX) */
						}
			}
			break;
		}
	}
	
	return NULL;
}
#endif /* !USECAPSTONE */

static void dumptokenline( u32 tokenAddress, u32 theNum, u32 currentPos )
{
	if ( gPass )
	{
		char				operand[100];
		char				instructionText[5];
		char				comment[10];

		if ( tokenAddress == 0 )
		{
			strcpy( operand, "0" );
		}
		else
		{
			s32					tokenOffset;
			token_t*			theToken;
			u32					offsetFromTokenStart;

			tokenOffset = tokenAddress - romstartoffset;
			theToken = find_token_by_pos( tokenOffset, &offsetFromTokenStart );
		
			if ( theToken == NULL )
				snprintf( operand, sizeof(operand), "$%+d", tokenOffset - currentPos );
			else if ( offsetFromTokenStart )
				snprintf( operand, sizeof(operand), "%s%+d", theToken->name, offsetFromTokenStart );
			else
				strcpy( operand, theToken->name );
		}

		dump_line( currentPos );
		u8 bytes[4];
		dump_buffer_hex( storebe( theNum, bytes ), 4, 2 );
		make_buffer_text( instructionText, bytes, 4 );

		snprintf( comment, sizeof(comment), "%08X", tokenAddress );
		printf( "%12s          %-8s %-39s \"%s\"\n", comment, "dc.l", operand, instructionText );
	}
	else
		linenum++;
}


static void dumpliteralline( s32 theNum, u32 currentPos )
{
	if ( gPass )
	{
		char				operand[20];
		char				instructionText[5];

		u32 absliteral;
		absliteral = (u32)labs( theNum );
		if
			(
					( absliteral < 10 )
				||	( absliteral % 10 == 0 && absliteral <= 100 )
				||	( absliteral % 100 == 0 && absliteral <= 10000 )
				||	( absliteral % 1000 == 0 )
			)
			snprintf( operand, sizeof(operand), "%d", theNum );
#ifdef USECAPSTONE
#else
		else if ( options & Disassemble_DollarHex )
			snprintf( operand, sizeof(operand), "$%X", theNum & 0x0ffffffff );
#endif
		else
			snprintf( operand, sizeof(operand), "0x%X", theNum & 0x0ffffffff );

		dump_line( currentPos );
		u8 bytes[4];
		dump_buffer_hex( storebe( theNum, bytes ), 4, 2 );
		make_buffer_text( instructionText, bytes, 4 );
		printf( "%12s          %-8s %-39s \"%s\"\n", "", "dc.l", operand, instructionText );
	}
	else
		linenum++;
}


static void dumpoffsetline( s32 theNum, u32 currentPos )
{
	if ( gPass )
	{
		char				operand[20];
		char				instructionText[5];

		if ( theNum < 100 )
			snprintf( operand, sizeof(operand), "%d", theNum );
#ifdef USECAPSTONE
#else
		else if ( options & Disassemble_DollarHex )
			snprintf( operand, sizeof(operand), "$%04X", theNum );
#endif
		else
			snprintf( operand, sizeof(operand), "0x%04X", theNum );
										
		dump_line( currentPos );
		u8 bytes[4];
		dump_buffer_hex( storebe( theNum, bytes ), 4, 2 );
		make_buffer_text( instructionText, bytes, 4 );
		printf( "%12s          %-8s %-39s \"%s\"\n", "", "dc.l", operand, instructionText );
	}
	else
		linenum++;
}


static u32 get_num32BE(u8* src) {
	return (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
}


void strip_spaces(char *d) {
	if (d) {
		for (char *s = d; ; s++) {
			if (*s != ' ')
				*d++ = *s;
			if (!*s) break;
		}
	}
}


static void dospr(u32 instruction, char *mnemonic, size_t sizemnemonic, char *operand, size_t sizeoperand, char *comment, size_t sizecomment)
{
	const char *reg = NULL;
	const char *nem = NULL;
	int imm = (((instruction >> 11) & 31) << 5) | ((instruction >> 16) & 31);
	bool to = ( (instruction & 0xfc0007ff) == 0x7c0003a6 );
	int r = ((instruction >> 21) & 31);
	int inc=0;

	if (!to) inc += scnprintf(operand+inc, sizeoperand-inc, "r%d", r);
	
	switch (imm) {
		case    0: reg = "MQ"     ; break;
		case    1: nem = "xer"    ; break;
		case    4: nem = "rtcu1"  ; break;
		case    5: nem = "rtcl1"  ; break;
		case    6: nem = "dec2"   ; break;
		case    8: nem = "lr"     ; break;
		case    9: nem = "ctr"    ; break;
		case   18: nem = "dsisr"  ; break;
		case   19: nem = "dar"    ; break;
		case   22: nem = "dec"    ; break; // dec2
		case   25: nem = "sdr1"   ; break;
		case   26: nem = "srr0"   ; break;
		case   27: nem = "srr1"   ; break;

		case  256: reg = "VRSAVE" ; break; // Vector save/restore register

		case  272: nem = "sprg"; reg = "0" ; break;
		case  273: nem = "sprg"; reg = "1" ; break;
		case  274: nem = "sprg"; reg = "2" ; break;
		case  275: nem = "sprg"; reg = "3" ; break;
//		case  276: nem = "sprg"; reg = "4" ; break;
//		case  277: nem = "sprg"; reg = "5" ; break;
		case  278: nem = "sprg"; reg = "6" ; break;
		case  279: nem = "sprg"; reg = "7" ; break;

		case  276: reg = "SCOMC"  ; break; // Scan Communications Register // Used in Quad G5 firmware
		case  277: reg = "SCOMD"  ; break; // Scan Communications Register // Used in Quad G5 firmware

		case  282: nem = "ear"    ; break;
		case  284: nem = "tbl"    ; break;
		case  285: nem = "tbu"    ; break;

		case  287: nem = "pvr"    ; break; // User monitor mode control register

		case  304: reg = "HSPRG0" ; break; // Hypervisor SPRG
		case  305: reg = "HSPRG1" ; break; // Hypervisor SPRG

		case  310: reg = "HDEC"   ; break; // Hypervisor Decrementer

		case  311: reg = "HIOR"   ; break; // Hypervisor Interrupt Offset // Used in Quad G5 firmware

		case  314: reg = "HSRR0"  ; break; // Hypervisor Save/Restore
		case  315: reg = "HSRR1"  ; break; // Hypervisor Save/Restore

		case  528: nem = "ibatu"; reg = "0" ; break;
		case  529: nem = "ibatl"; reg = "0" ; break;
		case  530: nem = "ibatu"; reg = "1" ; break;
		case  531: nem = "ibatl"; reg = "1" ; break;
		case  532: nem = "ibatu"; reg = "2" ; break;
		case  533: nem = "ibatl"; reg = "2" ; break;
		case  534: nem = "ibatu"; reg = "3" ; break;
		case  535: nem = "ibatl"; reg = "3" ; break;
		case  536: nem = "dbatu"; reg = "0" ; break;
		case  537: nem = "dbatl"; reg = "0" ; break;
		case  538: nem = "dbatu"; reg = "1" ; break;
		case  539: nem = "dbatl"; reg = "1" ; break;
		case  540: nem = "dbatu"; reg = "2" ; break;
		case  541: nem = "dbatl"; reg = "2" ; break;
		case  542: nem = "dbatu"; reg = "3" ; break;
		case  543: nem = "dbatl"; reg = "3" ; break;
		case  568: nem = "dbatu"; reg = "4" ; break;
		case  569: nem = "dbatl"; reg = "4" ; break;
		case  570: nem = "dbatu"; reg = "5" ; break;
		case  571: nem = "dbatl"; reg = "5" ; break;
		case  572: nem = "dbatu"; reg = "6" ; break;
		case  573: nem = "dbatl"; reg = "6" ; break;
		case  574: nem = "dbatu"; reg = "7" ; break;
		case  575: nem = "dbatl"; reg = "7" ; break;

		case  787: reg = "PMC1"   ; break; // Performance Counter
		case  788: reg = "PMC2"   ; break; // Performance Counter
		case  789: reg = "PMC3"   ; break; // Performance Counter
		case  790: reg = "PMC4"   ; break; // Performance Counter
		case  791: reg = "PMC5"   ; break; // Performance Counter
		case  792: reg = "PMC6"   ; break; // Performance Counter
		case  793: reg = "PMC7"   ; break; // Performance Counter
		case  794: reg = "PMC8"   ; break; // Performance Counter

		case  771: reg = "UPMC1"  ; break; // Performance Monitor Register
		case  772: reg = "UPMC2"  ; break; // Performance Monitor Register
		case  773: reg = "UPMC3"  ; break; // Performance Monitor Register
		case  774: reg = "UPMC4"  ; break; // Performance Monitor Register

		case  779: reg = "UMMCR0" ; break; // Monitor Control
		case  782: reg = "UMMCR1" ; break; // Monitor Control
		case  770: reg = "UMMCRA" ; break; // Monitor Control

		case  780: reg = "USIAR"  ; break; // Sampled Address Register
		case  781: reg = "USDAR"  ; break; // Sampled Address Register

		case  795: reg = "MMCR0"  ; break; // Monitor Control
		case  798: reg = "MMCR1"  ; break; // Monitor Control
		case  786: reg = "MMCRA"  ; break; // Monitor Control

		case  796: reg = "SIAR"   ; break; // Sampled Address Register
		case  797: reg = "SDAR"   ; break; // Sampled Address Register

		case  799: reg = "UIMC"   ; break; // IMC Array Address

		case  937: reg = "UPMC1"  ; break; // User performance monitor counter register
		case  938: reg = "UPMC2"  ; break; // User performance monitor counter register
		case  941: reg = "UPMC3"  ; break; // User performance monitor counter register
		case  942: reg = "UPMC4"  ; break; // User performance monitor counter register
		case  929: reg = "UPMC5"  ; break; // User performance monitor counter register
		case  930: reg = "UPMC6"  ; break; // User performance monitor counter register

		case  936: reg = "UMMCR0" ; break; // User monitor mode control register
		case  940: reg = "UMMCR1" ; break; // User monitor mode control register
		case  928: reg = "UMMCR2" ; break; // User monitor mode control register

		case  939: reg = "USIAR"  ; break; // User sampled instruction address register
		case  951: reg = "BAMR"   ; break; // Breakpoint address mask register

		case  952: reg = "MMCR0"  ; break; // Monitor mode control register
		case  956: reg = "MMCR1"  ; break; // Monitor mode control register
		case  944: reg = "MMCR2"  ; break; // Monitor mode control register

		case  953: reg = "PMC1"   ; break; // Performance monitor counter register
		case  954: reg = "PMC2"   ; break; // Performance monitor counter register
		case  957: reg = "PMC3"   ; break; // Performance monitor counter register
		case  958: reg = "PMC4"   ; break; // Performance monitor counter register
		case  945: reg = "PMC5"   ; break; // Performance monitor counter register
		case  946: reg = "PMC6"   ; break; // Performance monitor counter register

		case  955: reg = "SIAR"   ; break; // User monitor mode control register

//		case  976: reg = "DMISS"  ; break; // Software Table Search Register (603)
//		case  977: reg = "DCMP"   ; break; // Software Table Search Register (603)
//		case  978: reg = "HASH1"  ; break; // Software Table Search Register (603)
//		case  979: reg = "HASH2"  ; break; // Software Table Search Register (603)
//		case  980: reg = "IMISS"  ; break; // Software Table Search Register (603)
//		case  981: reg = "ICMP"   ; break; // Software Table Search Register (603)
//		case  982: reg = "RPA"    ; break; // Software Table Search Register (603)
		
		case  976: reg = "TRIG0"  ; break; // Trigger Register
		case  977: reg = "TRIG1"  ; break; // Trigger Register
		case  978: reg = "TRIG2"  ; break; // Trigger Register

		case  980: reg = "TLBMISS"; break; // Processor version register
		case  981: reg = "PTEHI"  ; break; // Sampled instruction address register
		case  982: reg = "PTELO"  ; break; // User monitor mode control register

		case  983: reg = "L3PM"   ; break; // The L3 private memory register

		case  984: reg = "L3ITCR0"; break; // L3 cache input timing control register
		case 1001: reg = "L3ITCR1"; break; // L3 cache input timing control register
		case 1002: reg = "L3ITCR2"; break; // L3 cache input timing control register
		case 1003: reg = "L3ITCR3"; break; // L3 cache input timing control register

		case 1000: reg = "L3OHCR" ; break; // L3 cache output hold control register

		case 1008: reg = "HID0"   ; break; // Hardware Implementation Register // Used in Quad G5 firmware
		case 1009: reg = "HID1"   ; break; // Hardware Implementation Register // Used in Quad G5 firmware
		case 1012: reg = "HID4"   ; break; // Hardware Implementation Register // Used in Quad G5 firmware
		case 1014: reg = "HID5"   ; break; // Hardware Implementation Register // Used in Quad G5 firmware

		case 1010: reg = "IABR"   ; break; // Instruction Address Breakpoint Register
		case 1011: reg = "ICTRL"  ; break; // Instruction cache and interrupt control register
		case 1013: reg = "DABR"   ; break; // Data Address Breakpoint Register // Used in Quad G5 firmware
		case 1015: reg = "DABRX"  ; break; // Data Address Breakpoint Register // Used in Quad G5 firmware
//		case 1014: reg = "MSSCR0" ; break; // Memory subsystem control register
//		case 1015: reg = "MSSSR0" ; break; // Memory subsystem status register
		case 1016: reg = "LDSTCR" ; break; // Load/store control register
		case 1017: reg = "L2CR"   ; break; // L2 Cache Control Register
		case 1018: reg = "L3CR"   ; break; // L3 cache control register
		case 1019: reg = "ICTC"   ; break; // Instruction Cache Throttling Control Register

		case 1023: reg = "PIR"    ; break; // Processor Identification Register // Used in Quad G5 firmware
			
		default:
			if (!to) inc += scnprintf(operand+inc, sizeoperand-inc, ",");
			inc += scnprintf(operand+inc, sizeoperand-inc, "%d", imm );
			if (to) inc += scnprintf(operand+inc, sizeoperand-inc, ",");
	}
	if (reg) {
		if (!to) inc += scnprintf(operand+inc, sizeoperand-inc, ",");
		inc += scnprintf(operand+inc, sizeoperand-inc, "%s", reg );
		if (to) inc += scnprintf(operand+inc, sizeoperand-inc, ",");
		if (!nem)
			snprintf( comment, sizecomment, "; %d", imm );
	}
	if (to) inc += scnprintf(operand+inc, sizeoperand-inc, "r%d", r);

	snprintf( mnemonic, sizemnemonic, "m%s%s", to ? "t" : "f", nem ? nem : "spr" );
}

#endif /* USEDISASSEMBLER */


void disassemble_lines(const u32 endPos, u32 currentPos)
{
#ifdef USEDISASSEMBLER

	#define comment3size 1000
	#define operandsize 100
	char				mnemonic[100];
	char				operand[100];
	char				comment[200];
	const char*			comment2;
	char				comment3[comment3size];
	char				instructionText[5];
	bool                gotInstruction;

	if (mac_rom)
	{
#ifdef USECAPSTONE
#else
		long adjust = romstartoffset - (size_t)&instructionMem;
#endif
		u32 instructionEnd = endPos & -4; /* 3->0 4->4 */

		while (currentPos < endPos)
		{
/* loop through bad PPC instructions */
			
			u32 firstHexPos = currentPos;
			currentPos = (currentPos + 4-1) & -4; /* 0->0 1->4 4->4 */

			if ( !gLastTokenIsAlias )
			{
				while (currentPos < instructionEnd)
				{
					get_mem((u8*)&instructionMem, sizeof(instructionMem));
					instruction = get_num32BE((u8*)&instructionMem);
					gotInstruction = FALSE;
					if ( instruction != 0xDEADBEEF && instruction != 0xFFFFFFFF )
					{
#ifdef USECAPSTONE
						cs_insn *insn = NULL;
						size_t count = cs_check() ?
							cs_disasm( handle, (uint8_t*)&instructionMem, sizeof(instructionMem), romstartoffset + currentPos, 0, &insn )
						:
							0;
						if ( count && insn ) {
							cs_free( insn, count );
							gotInstruction = TRUE;
						}
						else if (
							( (instruction & 0xfc0003fe) == 0x7c000010 ) || // subc
							( (instruction & 0xfc0003fe) == 0x7c0000d0 ) || // neg
							( (instruction & 0xfc0003fe) == 0x7c000194 ) || // addze
							( (instruction & 0xfc0003fe) == 0x7c0001d0 ) || // subfme
							( (instruction & 0xfc0003fe) == 0x7c0001d4 ) || // addmex
							( (instruction & 0xfc0007ff) == 0x7c0002a6 ) || // mfspr
							( (instruction & 0xfc0007ff) == 0x7c0003a6 ) || // mtspr
							( (instruction & 0xfc7fffff) == 0x7c000400 ) || // mcrxr
							( (instruction & 0xfc0007ff) == 0x7c00052a ) || // stswx
							( (instruction & 0xfc6007ff) == 0xfc000040 ) || // fcmpo
							0
						) {
							gotInstruction = TRUE;
						}
#else
						DisassemblerStatus disasmStat = Disassembler_OK ^ ppcDisassembler(
							&instructionMem,
							adjust + currentPos,
							options,
							mnemonic,
							operand,
							comment,
							NULL, /* refCon */
							EmptyDisassemblerLookups /* DisassemblerLookups lookupRoutine */
						);
						if ( instruction == 0x7C2002A6 ) /* this instruction "mfspr r1,MQ ; 0" has the Disassembler_InvField bit set because it is 601 specific */
						{
							disasmStat &= ~Disassembler_InvField;
						}
						if ( ( disasmStat & ( Disassembler_OK | Disassembler_InvRsvBits | Disassembler_InvField /*| Disassembler_Privileged*/ ) ) == 0 )
						{
							gotInstruction = TRUE;
						}
#endif
					}
					if ( gotInstruction ) {
						set_streampos( currentPos );
						break; /* got a good instruction */
					}

					currentPos += 4;
				} /* while */
			}
			
			if ( currentPos == instructionEnd )
				currentPos = endPos;

/* dump bad PPC instructions */
			dump_hex( firstHexPos, currentPos );

/* loop through good PPC instructions */
			while (currentPos < instructionEnd)
			{
				get_mem((u8*)&instructionMem, sizeof(instructionMem));
				instruction = get_num32BE((u8*)&instructionMem);
				if ( gLastTokenIsAlias )
				{
					u32 tokenAddress = instruction + currentPos + romstartoffset;
					dumptokenline( tokenAddress, instruction, currentPos );
					currentPos += 4;
					gLastTokenIsAlias = FALSE;
				}
				else
				{
					gotInstruction = FALSE;
					if ( instruction != 0xDEADBEEF && instruction != 0xFFFFFFFF )
					{
						comment3[0] = '\0';

#ifdef USECAPSTONE
						gDissasembledToken = NULL;

						cs_insn *insn = NULL;
						size_t count = cs_check() ?
							cs_disasm( handle, (uint8_t*)&instructionMem, sizeof(instructionMem), romstartoffset + currentPos, 0, &insn )
						:
							0;
						if ( count && insn ) {
							snprintf( mnemonic, sizeof(mnemonic), "%s", insn->mnemonic );
							snprintf( operand, sizeof(operand), "%s", insn->op_str );
							strip_spaces( operand );
							snprintf( comment, sizeof(comment), "" );
							
							cs_detail *detail = insn->detail;
							cs_ppc *ppc = &detail->ppc;
							cs_ppc_op *operands = ppc->operands;

/*
	uint8_t bo; ///< BO field of branch condition. UINT8_MAX if invalid.
	uint8_t bi; ///< BI field of branch condition. UINT8_MAX if invalid.
	ppc_cr_bit crX_bit; ///< CR field bit to test.
	ppc_reg crX;	    ///< The CR field accessed.
	ppc_br_hint hint;   ///< The encoded hint.
	ppc_pred pred_cr;   ///< CR-bit branch predicate
	ppc_pred pred_ctr;  ///< CTR branch predicate
	ppc_bh bh;	    ///< The BH field hint if any is present.
*/

							if (debugcapstone) {
								int inc = 0;
								inc += scnprintf( comment3+inc, comment3size-inc, " \\ nem:\"%s\" operands:\"%s\" id:%d=%s",
									insn->mnemonic,
									insn->op_str,
									insn->id,
									cs_insn_name(handle, insn->id)
								);
								if (ppc->bc.bo != UINT8_MAX)
									inc += scnprintf( comment3+inc, comment3size-inc, " bo:%d", (int)ppc->bc.bo );
								if (ppc->bc.bi != UINT8_MAX)
									inc += scnprintf( comment3+inc, comment3size-inc, " bi:%d", (int)ppc->bc.bi );
								if (ppc->bc.crX_bit != UINT8_MAX)
									inc += scnprintf( comment3+inc, comment3size-inc, " crX_bit:%s",
										ppc->bc.crX_bit == PPC_BI_LT ? "LT" :
										ppc->bc.crX_bit == PPC_BI_GT ? "GT" :
										ppc->bc.crX_bit == PPC_BI_Z  ? "Z" :
										ppc->bc.crX_bit == PPC_BI_SO ? "SO" : "unknown"
									);
								if (ppc->bc.crX)
									inc += scnprintf( comment3+inc, comment3size-inc, " cr%d", (int)ppc->bc.crX - PPC_REG_CR0 );
								if (ppc->bc.hint)
									inc += scnprintf( comment3+inc, comment3size-inc, " br_hint:%s",
										ppc->bc.hint == PPC_BR_RESERVED ? "RESERVED" :
										ppc->bc.hint == PPC_BR_NOT_TAKEN ? "NOT_TAKEN-" :
										ppc->bc.hint == PPC_BR_TAKEN  ? "TAKEN+" :
										ppc->bc.hint == PPC_BR_HINT_MASK ? "HINT:MASK" : "unknown"
									);
								if (ppc->bc.pred_cr != PPC_PRED_INVALID)
									inc += scnprintf( comment3+inc, comment3size-inc, " pred_cr:%s",
										ppc->bc.pred_cr == PPC_PRED_LT			? "LT" :
										ppc->bc.pred_cr == PPC_PRED_LE			? "LE" :
										ppc->bc.pred_cr == PPC_PRED_EQ			? "EQ" :
										ppc->bc.pred_cr == PPC_PRED_GE			? "GE" :
										ppc->bc.pred_cr == PPC_PRED_GT			? "GT" :
										ppc->bc.pred_cr == PPC_PRED_NE			? "NE" :
										ppc->bc.pred_cr == PPC_PRED_UN			? "UN" :
										ppc->bc.pred_cr == PPC_PRED_NU			? "NU" :
										ppc->bc.pred_cr == PPC_PRED_SO			? "SO" :
										ppc->bc.pred_cr == PPC_PRED_NS			? "NS" :
										ppc->bc.pred_cr == PPC_PRED_NZ			? "NZ" :
										ppc->bc.pred_cr == PPC_PRED_Z			? "Z" :
										ppc->bc.pred_cr == PPC_PRED_LT_MINUS	? "LT_MINUS" :
										ppc->bc.pred_cr == PPC_PRED_LE_MINUS	? "LE_MINUS" :
										ppc->bc.pred_cr == PPC_PRED_EQ_MINUS	? "EQ_MINUS" :
										ppc->bc.pred_cr == PPC_PRED_GE_MINUS	? "GE_MINUS" :
										ppc->bc.pred_cr == PPC_PRED_GT_MINUS	? "GT_MINUS" :
										ppc->bc.pred_cr == PPC_PRED_NE_MINUS	? "NE_MINUS" :
										ppc->bc.pred_cr == PPC_PRED_UN_MINUS	? "UN_MINUS" :
										ppc->bc.pred_cr == PPC_PRED_NU_MINUS	? "NU_MINUS" :
										ppc->bc.pred_cr == PPC_PRED_NZ_MINUS	? "NZ_MINUS" :
										ppc->bc.pred_cr == PPC_PRED_Z_MINUS		? "Z_MINUS" :
										ppc->bc.pred_cr == PPC_PRED_LT_PLUS		? "LT_PLUS" :
										ppc->bc.pred_cr == PPC_PRED_LE_PLUS		? "LE_PLUS" :
										ppc->bc.pred_cr == PPC_PRED_EQ_PLUS		? "EQ_PLUS" :
										ppc->bc.pred_cr == PPC_PRED_GE_PLUS		? "GE_PLUS" :
										ppc->bc.pred_cr == PPC_PRED_GT_PLUS		? "GT_PLUS" :
										ppc->bc.pred_cr == PPC_PRED_NE_PLUS		? "NE_PLUS" :
										ppc->bc.pred_cr == PPC_PRED_UN_PLUS		? "UN_PLUS" :
										ppc->bc.pred_cr == PPC_PRED_NU_PLUS		? "NU_PLUS" :
										ppc->bc.pred_cr == PPC_PRED_NZ_PLUS		? "NZ_PLUS" :
										ppc->bc.pred_cr == PPC_PRED_Z_PLUS		? "Z_PLUS" :
										ppc->bc.pred_cr == PPC_PRED_LT_RESERVED	? "LT_RESERVED" :
										ppc->bc.pred_cr == PPC_PRED_LE_RESERVED	? "LE_RESERVED" :
										ppc->bc.pred_cr == PPC_PRED_EQ_RESERVED	? "EQ_RESERVED" :
										ppc->bc.pred_cr == PPC_PRED_GE_RESERVED	? "GE_RESERVED" :
										ppc->bc.pred_cr == PPC_PRED_GT_RESERVED	? "GT_RESERVED" :
										ppc->bc.pred_cr == PPC_PRED_NE_RESERVED	? "NE_RESERVED" :
										ppc->bc.pred_cr == PPC_PRED_UN_RESERVED	? "UN_RESERVED" :
										ppc->bc.pred_cr == PPC_PRED_NU_RESERVED	? "NU_RESERVED" :
										ppc->bc.pred_cr == PPC_PRED_NZ_RESERVED	? "NZ_RESERVED" :
										ppc->bc.pred_cr == PPC_PRED_Z_RESERVED	? "Z_RESERVED" :
										ppc->bc.pred_cr == PPC_PRED_SPE			? "SPE" :
										ppc->bc.pred_cr == PPC_PRED_BIT_SET		? "BIT_SET" :
										ppc->bc.pred_cr == PPC_PRED_BIT_UNSET	? "BIT_UNSET" : "unknown"
									);
								if (ppc->bc.pred_ctr != PPC_PRED_INVALID)
									inc += scnprintf( comment3+inc, comment3size-inc, " pred_ctr:%s",
										ppc->bc.pred_ctr == PPC_PRED_LT				? "LT" :
										ppc->bc.pred_ctr == PPC_PRED_LE				? "LE" :
										ppc->bc.pred_ctr == PPC_PRED_EQ				? "EQ" :
										ppc->bc.pred_ctr == PPC_PRED_GE				? "GE" :
										ppc->bc.pred_ctr == PPC_PRED_GT				? "GT" :
										ppc->bc.pred_ctr == PPC_PRED_NE				? "NE" :
										ppc->bc.pred_ctr == PPC_PRED_UN				? "UN" :
										ppc->bc.pred_ctr == PPC_PRED_NU				? "NU" :
										ppc->bc.pred_ctr == PPC_PRED_SO				? "SO" :
										ppc->bc.pred_ctr == PPC_PRED_NS				? "NS" :
										ppc->bc.pred_ctr == PPC_PRED_NZ				? "NZ" :
										ppc->bc.pred_ctr == PPC_PRED_Z				? "Z" :
										ppc->bc.pred_ctr == PPC_PRED_LT_MINUS		? "LT_MINUS" :
										ppc->bc.pred_ctr == PPC_PRED_LE_MINUS		? "LE_MINUS" :
										ppc->bc.pred_ctr == PPC_PRED_EQ_MINUS		? "EQ_MINUS" :
										ppc->bc.pred_ctr == PPC_PRED_GE_MINUS		? "GE_MINUS" :
										ppc->bc.pred_ctr == PPC_PRED_GT_MINUS		? "GT_MINUS" :
										ppc->bc.pred_ctr == PPC_PRED_NE_MINUS		? "NE_MINUS" :
										ppc->bc.pred_ctr == PPC_PRED_UN_MINUS		? "UN_MINUS" :
										ppc->bc.pred_ctr == PPC_PRED_NU_MINUS		? "NU_MINUS" :
										ppc->bc.pred_ctr == PPC_PRED_NZ_MINUS		? "NZ_MINUS" :
										ppc->bc.pred_ctr == PPC_PRED_Z_MINUS		? "Z_MINUS" :
										ppc->bc.pred_ctr == PPC_PRED_LT_PLUS		? "LT_PLUS" :
										ppc->bc.pred_ctr == PPC_PRED_LE_PLUS		? "LE_PLUS" :
										ppc->bc.pred_ctr == PPC_PRED_EQ_PLUS		? "EQ_PLUS" :
										ppc->bc.pred_ctr == PPC_PRED_GE_PLUS		? "GE_PLUS" :
										ppc->bc.pred_ctr == PPC_PRED_GT_PLUS		? "GT_PLUS" :
										ppc->bc.pred_ctr == PPC_PRED_NE_PLUS		? "NE_PLUS" :
										ppc->bc.pred_ctr == PPC_PRED_UN_PLUS		? "UN_PLUS" :
										ppc->bc.pred_ctr == PPC_PRED_NU_PLUS		? "NU_PLUS" :
										ppc->bc.pred_ctr == PPC_PRED_NZ_PLUS		? "NZ_PLUS" :
										ppc->bc.pred_ctr == PPC_PRED_Z_PLUS			? "Z_PLUS" :
										ppc->bc.pred_ctr == PPC_PRED_LT_RESERVED	? "LT_RESERVED" :
										ppc->bc.pred_ctr == PPC_PRED_LE_RESERVED	? "LE_RESERVED" :
										ppc->bc.pred_ctr == PPC_PRED_EQ_RESERVED	? "EQ_RESERVED" :
										ppc->bc.pred_ctr == PPC_PRED_GE_RESERVED	? "GE_RESERVED" :
										ppc->bc.pred_ctr == PPC_PRED_GT_RESERVED	? "GT_RESERVED" :
										ppc->bc.pred_ctr == PPC_PRED_NE_RESERVED	? "NE_RESERVED" :
										ppc->bc.pred_ctr == PPC_PRED_UN_RESERVED	? "UN_RESERVED" :
										ppc->bc.pred_ctr == PPC_PRED_NU_RESERVED	? "NU_RESERVED" :
										ppc->bc.pred_ctr == PPC_PRED_NZ_RESERVED	? "NZ_RESERVED" :
										ppc->bc.pred_ctr == PPC_PRED_Z_RESERVED		? "Z_RESERVED" :
										ppc->bc.pred_ctr == PPC_PRED_SPE			? "SPE" :
										ppc->bc.pred_ctr == PPC_PRED_BIT_SET		? "BIT_SET" :
										ppc->bc.pred_ctr == PPC_PRED_BIT_UNSET		? "BIT_UNSET" : "unknown"
									);
								if (ppc->bc.bh != PPC_BH_INVALID)
									inc += scnprintf( comment3+inc, comment3size-inc, " bh:%s",
										ppc->bc.bh == PPC_BH_SUBROUTINE_RET			? "SUBROUTINE_RET" :
										ppc->bc.bh == PPC_BH_NO_SUBROUTINE_RET		? "NO_SUBROUTINE_RET" :
										ppc->bc.bh == PPC_BH_NOT_PREDICTABLE		? "NOT_PREDICTABLE" :
										ppc->bc.bh == PPC_BH_RESERVED				? "RESERVED" : "unknown"
									);

								if (ppc->update_cr0)
									inc += scnprintf( comment3+inc, comment3size-inc, " update_cr0");

								inc += scnprintf( comment3+inc, comment3size-inc, " groups:" );
								for ( int i = 0; i < detail->groups_count; i++ ) {
									inc += scnprintf( comment3+inc, comment3size-inc, "%s%d=%s", i?",":"",
										detail->groups[i],
										cs_group_name( handle, detail->groups[i] )
									);
								}
								inc += scnprintf( comment3+inc, comment3size-inc, " ops:" );
								for ( int i = 0; i < ppc->op_count; i++ ) {
									cs_ppc_op *ppc_op = &operands[i];
									inc += scnprintf( comment3+inc, comment3size-inc, "%s", i?",":"" );
									switch (ppc_op->type) {
										case PPC_OP_INVALID: inc += scnprintf( comment3+inc, comment3size-inc, "invalid" ); break;
										case PPC_OP_REG    : inc += scnprintf( comment3+inc, comment3size-inc, "%d=%s", ppc_op->reg, cs_reg_name( handle, ppc_op->reg ) ); break;
										case PPC_OP_IMM    : inc += scnprintf( comment3+inc, comment3size-inc, "0x%llx", ppc_op->imm ); break;
										case PPC_OP_MEM    : inc += scnprintf( comment3+inc, comment3size-inc, "0x%x(%d=%s,%d=%s)", ppc_op->mem.disp, ppc_op->mem.base, cs_reg_name( handle, ppc_op->mem.base ), ppc_op->mem.offset, cs_reg_name( handle, ppc_op->mem.offset ) ); break;
									}
								}
							} // if debugcapstone

							if (
								insn->id == PPC_INS_ALIAS_MTDBATU || insn->id == PPC_INS_ALIAS_MTDBATL || insn->id == PPC_INS_ALIAS_MTIBATU || insn->id == PPC_INS_ALIAS_MTIBATL ||
								insn->id == PPC_INS_ALIAS_MFDBATU || insn->id == PPC_INS_ALIAS_MFDBATL || insn->id == PPC_INS_ALIAS_MFIBATU || insn->id == PPC_INS_ALIAS_MFIBATL
							) {
								// 0,1,2,3, ... are not differentiated by fields of insn and detail so don't override
								gotInstruction = TRUE;
							}
							else { // override operands
								operand[0] = '\0';
								int inc = 0;
								int out_op_count = 0;

								// we need to parse branch instructions in case they point to a special token like (defer) which modifies the meaning of subsequent bytes
								if (
									insn->id == PPC_INS_BC      || // bx
									insn->id == PPC_INS_BCA     || // bcx
									insn->id == PPC_INS_BCCTR	||
									insn->id == PPC_INS_BCCTRL	||
									insn->id == PPC_INS_BCL		||
									insn->id == PPC_INS_BCLA	||
									insn->id == PPC_INS_BCLR	||
									insn->id == PPC_INS_BCLRL	||

									insn->id == PPC_INS_B       || // bx
									insn->id == PPC_INS_BA      || // bx
									insn->id == PPC_INS_BL      || // bx
									insn->id == PPC_INS_BLA     || // bx
/*
									insn->id == PPC_INS_ALIAS_BDZ     || // bcx
//									insn->id == PPC_INS_ALIAS_BDZA    || // bcx
									insn->id == PPC_INS_ALIAS_BDZL    || // bcx
									insn->id == PPC_INS_ALIAS_BDZLA   || // bcx

									insn->id == PPC_INS_ALIAS_BDNZ    || // bcx
//									insn->id == PPC_INS_ALIAS_BDNZA   || // bcx
									insn->id == PPC_INS_ALIAS_BDNZL   || // bcx
									insn->id == PPC_INS_ALIAS_BDNZLA  || // bcx


									insn->id == PPC_INS_ALIAS_BDZT    || // bcx
									insn->id == PPC_INS_ALIAS_BDZTA   || // bcx
									insn->id == PPC_INS_ALIAS_BDZTL   || // bcx
									insn->id == PPC_INS_ALIAS_BDZTLA  || // bcx

									insn->id == PPC_INS_ALIAS_BDNZT   || // bcx
									insn->id == PPC_INS_ALIAS_BDNZTA  || // bcx
									insn->id == PPC_INS_ALIAS_BDNZTL  || // bcx
									insn->id == PPC_INS_ALIAS_BDNZTLA || // bcx

									
									insn->id == PPC_INS_ALIAS_BDZF    || // bcx
									insn->id == PPC_INS_ALIAS_BDZFA   || // bcx
									insn->id == PPC_INS_ALIAS_BDZFL   || // bcx
									insn->id == PPC_INS_ALIAS_BDZFLA  || // bcx

									insn->id == PPC_INS_ALIAS_BDNZF   || // bcx
									insn->id == PPC_INS_ALIAS_BDNZFA  || // bcx
									insn->id == PPC_INS_ALIAS_BDNZFL  || // bcx
									insn->id == PPC_INS_ALIAS_BDNZFLA || // bcx
*/
									0
								) {
									bool bx     = (instruction & 0xfc000000) == 0x48000000; // bx (ba, bl, bla)
									bool bcx    = (instruction & 0xfc000000) == 0x40000000; // bcx (bca, bcl, bcla)
									bool bcctrx = (instruction & 0xfc00fffe) == 0x4c000420; // bcctrx (bcctr, bcctrl)
									bool bclrx  = (instruction & 0xfc00fffe) == 0x4c000020; // bclrx (bclr, bclrl)
									bool LK     = (instruction & 1) != 0;
									bool AA     = (instruction & 2) != 0; // can only be true for bx and bcx; always false for bcctrx and bclrx
									bool y      = 0;
									bool checkCTR = 0;

									int b_op_count = 0;
									
									if ( bcx || bcctrx || bclrx ) {
										int BO = (instruction >> 21) & 31;
										int BI = (instruction >> 16) & 31;
										int cr = BI >> 2;
										int cond = BI & 3;
										bool checkCR = !(BO & 0x10); // 0
										bool checkCRisT = (BO & 8);  // 1
										checkCTR = !(BO & 4);        // 2
										bool checkCTRis0 = (BO & 2); // 3
										y = (BO & 1);                // 4
										bool croperand = false;
										bool condasis = false;

										if (BO != 0b10100 || BI != 0) {
											if ( !checkCR && !checkCTR ) {
												if (checkCTRis0) {
													inc += scnprintf( operand+inc, operandsize-inc, "0x%x", BO );
													condasis = true;
												} else {
													// For bcx, if not checking CR or CTR then it's ALWAYS
													inc += scnprintf( operand+inc, operandsize-inc, "ALWAYS" );
												}
												b_op_count++;
											}

											if (
												( checkCR && !checkCTR && cr ) || // bcx with not cr0
												(!checkCR && !checkCTR) || // bc ALWAYS
												( checkCR && checkCTR ) // bdx with condition check
											) {
												inc += scnprintf( operand+inc, operandsize-inc, "%scr%d", b_op_count ? "," : "", cr );
												b_op_count++;

												if (
													(!checkCR && !checkCTR) || // bc ALWAYS
													(checkCR && checkCTR) // bdx with condition check
												) {
													// if checking CTR then it's probably a bdx mnemonic without conditional, so add condition.
													inc += scnprintf( operand+inc, operandsize-inc, "_%s",
														(condasis || checkCRisT) && cond == 0 ? "LT" :
														(condasis || checkCRisT) && cond == 1 ? "GT" :
														(condasis || checkCRisT) && cond == 2 ? "EQ" :
														(condasis || checkCRisT) && cond == 3 ? "SO" :
														!checkCRisT && cond == 0 ? "GE" :
														!checkCRisT && cond == 1 ? "LE" :
														!checkCRisT && cond == 2 ? "NE" :
														!checkCRisT && cond == 3 ? "NS" :
														"??"
													);
													croperand = true;
													b_op_count++;
												}
											}
										}

										snprintf( mnemonic, sizeof(mnemonic), "b%s%s%s%s%s%s",
											(checkCTR && checkCTRis0) ? "dz" : "",
											(checkCTR && !checkCTRis0) ? "dnz" : "",

											(!checkCTR && croperand) ? "c" :
											(!checkCTR && checkCR && checkCRisT && cond == 0) ? "lt" :
											(!checkCTR && checkCR && checkCRisT && cond == 1) ? "gt" :
											(!checkCTR && checkCR && checkCRisT && cond == 2) ? "eq" :
											(!checkCTR && checkCR && checkCRisT && cond == 3) ? "so" :
											(!checkCTR && checkCR && !checkCRisT && cond == 0) ? "ge" :
											(!checkCTR && checkCR && !checkCRisT && cond == 1) ? "le" :
											(!checkCTR && checkCR && !checkCRisT && cond == 2) ? "ne" :
											(!checkCTR && checkCR && !checkCRisT && cond == 3) ? "ns" :
											(!checkCTR && checkCR) ? "??" : "",

											bcx    ? "" :
											bcctrx ? "ctr" :
											bclrx  ? "lr" :
											"??",

											LK ? "l" : "",
											AA ? "a" : ""
										);

									} // if bcx || bcctrx || bclrx

									int BD = 0;
									if ( bcx || bx ) {
										BD = bcx ? (int16_t)(instruction & ~3) : ((int32_t)((instruction & ~3) << 6) >> 6);
										int64_t imm;
										
										if ( AA ) {
											imm = (uint32_t)BD;
										}
										else {
											imm = (int64_t)currentPos + romstartoffset + BD;
											if ((uint32_t(imm >> 32)) + 1 == 0) {
												snprintf( comment, sizeof(comment), "; 0x%08X", (uint32_t)imm );
											}
											else {
												snprintf( comment, sizeof(comment), "; 0x%08llX", imm );
											}
										}

										bool istoken = FALSE;
										gDissasembledToken = find_token_by_pos( (u32)(imm - romstartoffset), &gDissasembledTokenOffset );
										if (gDissasembledToken)
										{
											istoken = imm - romstartoffset <= gLastPos;
											if (!istoken) {
												token_t* nextToken = find_next_token_by_pos( gLastPos );
												istoken = nextToken && ( imm - romstartoffset >= nextToken->execution_pos );
											}
										}
										if ( istoken ) {
											if ( gDissasembledTokenOffset == 0 )
												inc += scnprintf( operand+inc, operandsize-inc, "%s%s", b_op_count ? "," : "", gDissasembledToken->name );
											else {
												inc += scnprintf( operand+inc, operandsize-inc, "%s%s%+d", b_op_count ? "," : "", gDissasembledToken->name, gDissasembledTokenOffset );
												gDissasembledToken = NULL;
											}
											if ( AA ) {
												snprintf( comment, sizeof(comment), "; 0x%08llX", imm );
											}
										}
										else {
											if ( AA ) {
												inc += scnprintf( operand+inc, operandsize-inc, "%s0x%08llX", b_op_count ? "," : "", imm );
											}
											else {
												inc += scnprintf( operand+inc, operandsize-inc, "%s$%+lld", b_op_count ? "," : "", imm - (romstartoffset + currentPos) );
											}
										}
									} // if bcx || bx

									char hint[2];
									hint[0] = '\0';
									size_t nemlen = strlen(mnemonic);
									if ( nemlen && (mnemonic[nemlen-1] == '+' || mnemonic[nemlen-1] == '-' ) ) {
										snprintf(hint, sizeof(hint), "%c", mnemonic[nemlen-1] );
									}
									char newhint = BD < 0 ? '-' : '+';
									
									if ( y && hint[0] && hint[0] != newhint ) {
										fprintf(stderr, "Line %d # Capstone problem: Instruction %08X %s should have branch hint \"%c\"\n", linenum, instruction, mnemonic, newhint);
										snprintf(hint, sizeof(hint), "%c", newhint);
										nemlen--;
										snprintf(mnemonic + nemlen, sizeof(mnemonic)-nemlen, "%s", hint);
										nemlen++;
									}
									else if (y && !hint[0]) {
										// capstone problem
										// 41AA AAAA beqa cr2,0xFFFFAAA8 \ nem:"beqa" operands:"cr2, 0xffffffffffffaaa8" id:931=bca bc:EQ cr0:0 groups: ops:4=cr2,0xffffffffffffaaa8
										fprintf(stderr, "Line %d # Capstone problem: Instruction %08X %s is missing branch hint \"%c\"\n", linenum, instruction, mnemonic, newhint);
										snprintf(hint, sizeof(hint), "%c", newhint);
										snprintf(mnemonic + nemlen, sizeof(mnemonic)-nemlen, "%s", hint);
										nemlen++;
									}
									else if (!y && hint[0]) {
										fprintf(stderr, "Line %d # Capstone problem: Instruction %08X %s should not have branch hint \"%s\"\n", linenum, instruction, mnemonic, hint);
										hint[0] = '\0';
										nemlen--;
										mnemonic[nemlen] = '\0';
									}

									if ( !checkCTR && mnemonic[1] == 'd' ) {
										// if not checking CTR and the mnemonic is bdx then replace with bc because capstone is wrong:
										// 4280 001C bdnz 0x34c \ id:25=bdnz cr0:0 groups: ops:0x34c
										// Should be more like the following:
										// 4280 001C bc ALWAYS,cr0_LT,$+28
										// the cr0_LT is probably redundant, but we'll add it anyway.
										
										// also affects bdzl, bdnzfl

										fprintf(stderr, "Line %d # Capstone problem: Changing instruction %08X from %s", linenum, instruction, mnemonic );
										snprintf( mnemonic, sizeof(mnemonic), "%s%s%s%s", bx ? "b" : bcx ? "bc" : bcctrx ? "bcctr" : bclrx ? "bclr" : "b?", LK ? "l" : "", AA ? "a" : "", hint );
										fprintf(stderr, " to %s\n", mnemonic );
									}
									
								} // branch instructions
								else if (gPass) {
									// we don't need to parse the following instructions for the first pass
									
									if (insn->id == PPC_INS_MTSPR || insn->id == PPC_INS_MFSPR) {
										dospr(instruction, mnemonic, sizeof(mnemonic), operand, sizeof(operand), comment, sizeof(comment));
									}
									else if (insn->id == PPC_INS_SLDI) {
										// sldi rA,rS,n (n < 64)          rldicr rA,rS,n,63 – n
										// capstone doesn't include the last operand
										// 7A94 07C6 sldi r20,r20 \ nem:"sldi" operands:"r20, r20, 0x20" id:935=sldi cr0:0 groups: ops:64=r20,64=r20
										if ((instruction & 0xfc00001c) == 0x78000004) {
											int sh, me, n;
											sh = ((instruction >> 11) & 31) | (((instruction >> 1) & 1) << 5);
											me = ((instruction >>  6) & 31) | (((instruction >> 5) & 1) << 5);
											n = sh;
											if (me == 63 - n) {
												snprintf( operand, sizeof(operand), "r%d,r%d,%d", (instruction >> 16) & 31, (instruction >> 21) & 31, n );
											}
											else {
												fprintf(stderr, "Line %d # Capstone problem: instruction %08X %s is not sldi (wrong shift)", linenum, instruction, mnemonic );
											}
										}
										else {
											fprintf(stderr, "Line %d # Capstone problem: instruction %08X %s is not sldi (wrong bits)", linenum, instruction, mnemonic );
										}
									}
									else {
										// do each operand seperately
										for ( int i = 0; i < ppc->op_count; i++ ) {
									
											cs_ppc_op *ppc_op;

											if ( strncmp(mnemonic, "subf", 4) == 0 && (insn->id == PPC_INS_SUBF || insn->id == PPC_INS_SUBFC) ) {
												snprintf( mnemonic, sizeof(mnemonic), "sub%s%s",
													insn->id == PPC_INS_SUBF ? "" : "c",
													ppc->update_cr0 ? "." : ""
												);
												ppc_op = &operands[i == 0 ? 0 : i == 1 ? 2 : 1 ];
											}
											else {
												ppc_op = &operands[i];
											}
										
											char oneop[100];
											oneop[0] = '\0';
										
											switch (ppc_op->type) {

												case PPC_OP_REG:
												{
													if ( ppc_op->reg >= PPC_REG_F0 && ppc_op->reg <= PPC_REG_F31 ) {
														snprintf( oneop, sizeof(oneop), "fp%d", ppc_op->reg - PPC_REG_F0 );
													}
													else if (
														(
															( i == 0 && (
																insn->id == PPC_INS_DCBF   ||
																insn->id == PPC_INS_DCBZ   ||
																insn->id == PPC_INS_DCBST  ||
																insn->id == PPC_INS_DCBI   ||
																insn->id == PPC_INS_DCBT   ||
																insn->id == PPC_INS_DCBTST ||
																insn->id == PPC_INS_ICBI   ||
																0
															) ) ||
															( i == 1 && (
																insn->id == PPC_INS_LHZX   ||
																insn->id == PPC_INS_LWZX   ||
																insn->id == PPC_INS_LHBRX  ||
																insn->id == PPC_INS_LWARX  ||
																insn->id == PPC_INS_LWBRX  ||
																insn->id == PPC_INS_STHX   ||
																insn->id == PPC_INS_STWX   ||
																insn->id == PPC_INS_STWCX  ||
																insn->id == PPC_INS_STHBRX ||
																insn->id == PPC_INS_STWBRX ||
																0
															) )
														)
														&& ppc_op->reg == PPC_REG_R0
													) {
														snprintf( oneop, sizeof(oneop), "0" );
														// r0 should remain as default 0
													}
													else if (
														insn->id == PPC_INS_OR && i == 2 && ppc->op_count == 3 &&
														operands[1].type == PPC_OP_REG && operands[1].reg == ppc_op->reg
													) {
														// 7C00 0379 or. r0,r0,r0 \ id:406=or cr0:1 groups: ops:44=r0,44=r0,44=r0
														// but other registers already are converted to mr
														memcpy( mnemonic, "mr", 2 ); // use memcpy to keep the .
													}
													else if (
														insn->id == PPC_INS_NOR && i == 2 && ppc->op_count == 3 &&
														operands[1].type == PPC_OP_REG && operands[1].reg == ppc_op->reg
													) {
														// 7E94 A0F8 nor r20,r20,r20 \ id:405=nor cr0:0 groups: ops:64=r20,64=r20,64=r20
														memcpy( mnemonic, "not", 3 ); // use memcpy to keep the .
													}
													else if (
														insn->id == PPC_INS_CRAND  ||
														insn->id == PPC_INS_CRANDC ||
														insn->id == PPC_INS_CREQV  ||
														insn->id == PPC_INS_ALIAS_CRSET  ||
														insn->id == PPC_INS_CRNAND ||
														insn->id == PPC_INS_CRNOR  ||
														insn->id == PPC_INS_ALIAS_CRNOT  ||
														insn->id == PPC_INS_CROR   ||
														insn->id == PPC_INS_ALIAS_CRMOVE ||
														insn->id == PPC_INS_CRORC  ||
														insn->id == PPC_INS_CRXOR  ||
														insn->id == PPC_INS_ALIAS_CRCLR  ||
														0
													) {
														int cr;
														int cond;
														int val;
														if ( ppc_op->reg >= PPC_REG_CR0EQ ) {
															val = ppc_op->reg - PPC_REG_CR0EQ;
															cr = val & 7;
															cond = val >> 3;
															snprintf( oneop, sizeof(oneop), "cr%d_%s",
																cr,
																cond == 0 ? "EQ" :
																cond == 1 ? "GT" :
																cond == 2 ? "LT" :
																cond == 3 ? "SO" : "??"
															);
														} else {
															// old capstone doesn't distinguish condition register bits
															// 4C40 2342 crorc 2,0,4 \ id:60=crorc cr0:0 groups: ops:46=r2,44=r0,48=r4
															val = ppc_op->reg - PPC_REG_R0;
															cr = val >> 2;
															cond = val & 3;
															snprintf( oneop, sizeof(oneop), "cr%d_%s",
																cr,
																cond == 0 ? "LT" :
																cond == 1 ? "GT" :
																cond == 2 ? "EQ" :
																cond == 3 ? "SO" : "??"
															);
														}
													}
													else if (
														insn->id == PPC_INS_RLWINM &&
														i == 1 &&
														(strncmp(mnemonic, "slwi", 4) == 0 || strncmp(mnemonic, "srwi", 4) == 0)
													) {
														snprintf( oneop, sizeof(oneop), "r%d", (instruction >> 21) & 31 );
													}
													else {
														snprintf( oneop, sizeof(oneop), "%s", cs_reg_name( handle, ppc_op->reg ) );
													}
												}
												break;
												
												case PPC_OP_IMM:
												{
													int64_t imm = ppc_op->imm;
											
													if (
														insn->id == PPC_INS_ALIAS_LI     ||
														insn->id == PPC_INS_CMPWI  ||
														insn->id == PPC_INS_CMPDI  ||
														insn->id == PPC_INS_MULLI  ||
														insn->id == PPC_INS_ALIAS_TWGTI  ||
														insn->id == PPC_INS_SUBFIC ||
														( i == 1 && (
															insn->id == PPC_INS_ALIAS_TWLTI  ||
															insn->id == PPC_INS_ALIAS_TWEQI  ||
															insn->id == PPC_INS_ALIAS_TWGTI  ||
															insn->id == PPC_INS_ALIAS_TWNEI  ||
															insn->id == PPC_INS_ALIAS_TWLLTI ||
															insn->id == PPC_INS_ALIAS_TWLGTI ||
															insn->id == PPC_INS_ALIAS_TWUI   ||
															insn->id == PPC_INS_ALIAS_TDLTI  ||
															insn->id == PPC_INS_ALIAS_TDEQI  ||
															insn->id == PPC_INS_ALIAS_TDGTI  ||
															insn->id == PPC_INS_ALIAS_TDNEI  ||
															insn->id == PPC_INS_ALIAS_TDLLTI ||
															insn->id == PPC_INS_ALIAS_TDLGTI ||
															insn->id == PPC_INS_ALIAS_TDUI
														) ) ||
														( i == 2 && (
															insn->id == PPC_INS_TWI    ||
															insn->id == PPC_INS_TDI
														) )
													) {
														imm = (int16_t)imm;
														goto doimm;
													}

													else if (
														insn->id == PPC_INS_ALIAS_LIS   ||
														insn->id == PPC_INS_ORIS  ||
														insn->id == PPC_INS_ANDIS ||
														insn->id == PPC_INS_XORIS ||
														0
													) {
														if (imm >= 0 && imm < 10) {
															snprintf( oneop, sizeof(oneop), "%lld", imm );
														}
														else {
															snprintf( oneop, sizeof(oneop), "0x%04llX", imm & 0x0FFFF );
														}
													}

													else if (
														insn->id == PPC_INS_ALIAS_ROTLWI && imm > 16
													) {
														snprintf( oneop, sizeof(oneop), "%lld", 32 - imm );
														mnemonic[3] = 'r';
													}
													
													else if (
														insn->id == PPC_INS_MFSR    ||
														insn->id == PPC_INS_MTSR    ||
														insn->id == PPC_INS_SRWI    ||
														insn->id == PPC_INS_SLWI    ||
														insn->id == PPC_INS_SRWI    ||
														insn->id == PPC_INS_LSWI    ||
														insn->id == PPC_INS_STSWI   ||
														insn->id == PPC_INS_MTCRF   ||
														insn->id == PPC_INS_SRAWI   ||
														insn->id == PPC_INS_RLWNM   ||
														insn->id == PPC_INS_RLDCL   ||
														insn->id == PPC_INS_RLDCR   ||
														insn->id == PPC_INS_ALIAS_CLRLWI  ||
														insn->id == PPC_INS_ALIAS_CLRLDI  ||
														insn->id == PPC_INS_ALIAS_ROTLDI  ||
														insn->id == PPC_INS_ALIAS_ROTLWI  ||
														insn->id == PPC_INS_VSLDOI  ||
														insn->id == PPC_INS_VCFSX   ||
														insn->id == PPC_INS_VCFUX   ||
														insn->id == PPC_INS_VCTSXS  ||
														insn->id == PPC_INS_VCTUXS  ||
														insn->id == PPC_INS_VSPLTB  ||
														insn->id == PPC_INS_VSPLTH  ||
														insn->id == PPC_INS_VSPLTW  ||
														0
													) {
														snprintf( oneop, sizeof(oneop), "%lld", imm );
													}
												
													else if ( insn->id == PPC_INS_MFTB && imm == 268 ) {
														// why does capstone produce this?
														// 7C8C 42E6 mftb r4,0x10c \ id:376=mftb cr0:0 groups: ops:48=r4,0x10c
													}

													else if ( insn->id == PPC_INS_SYNC && imm == 0 ) {
														// why does capstone produce this?
														// 7C00 04AC sync 0 \ id:606=sync cr0:0 groups: ops:0x0
													}

													else if ( i == 2 && (
														insn->id == PPC_INS_ADDI  ||
														insn->id == PPC_INS_ADDIC ||
														insn->id == PPC_INS_ADDIS
													) && imm & 0x8000 ) {
														// adding a negative value is a subtract
														// "li" is PPC_INS_ADDI with two operands, so check i == 2 to make sure this is a three operand add instruciton
														memcpy( mnemonic, "sub", 3 ); // use memcpy to copy the i, ic, or is and .
														imm = -(int16_t)imm;
														goto doimm;
													}

													else if (
														insn->id == PPC_INS_ADDIS
													) {
														// shifted operands are easier to understand if unsigned
														imm = (uint16_t)imm;
														goto doimm;
													}

													else if ( i == 0 && (
														insn->id == PPC_INS_TW  ||
														insn->id == PPC_INS_TWI ||
														insn->id == PPC_INS_TD  ||
														insn->id == PPC_INS_TDI
													) ) {
														// capstone doesn't list flags of twi.
														// 0D38 4339 twi 9,r24,0x4339 \ nem:"twi" operands:"9, r24, 0x4339" id:621=twi cr0:0 groups: ops:0x9,68=r24,0x4339
														// should be:
														// 0D38 4339 twi GT|HI,r24,0x4339
														int TO = (instruction >> 21) & 31;
														
														const int LT = 16;
														const int GT = 8;
														const int EQ = 4;
														const int LOW = 2;
														const int HI = 1;
														
														const char *cond = NULL;
														switch (TO) {
															case LT     : cond = "lt" ; break;
															case LT|EQ  : cond = "le" ; break;
															case EQ     : cond = "eq" ; break;
															case GT|EQ  : cond = "ge" ; break;
															case GT     : cond = "gt" ; break;
															case LT|GT  : cond = "ne" ; break;
															case LOW    : cond = "llt"; break;
															case EQ|LOW : cond = "lle"; break;
															case EQ|HI  : cond = "lge"; break;
															case HI     : cond = "lgt"; break;
															case 31     : cond = "u"  ; break;
														}
														
														if ( cond ) {
															switch (insn->id) {
																case PPC_INS_TW  : snprintf(mnemonic, sizeof(mnemonic), "tw%s" , cond); break;
																case PPC_INS_TWI : snprintf(mnemonic, sizeof(mnemonic), "tw%si", cond); break;
																case PPC_INS_TD  : snprintf(mnemonic, sizeof(mnemonic), "td%s" , cond); break;
																case PPC_INS_TDI : snprintf(mnemonic, sizeof(mnemonic), "td%si", cond); break;
															}
														}
														else if (TO) {
															int numflags = 0;
															int inc = 0;
															const char *flags[] = { "LT", "GT", "EQ", "LOW", "HI"};
															for (int f = 0; f < 5; f++ ) {
																if ( TO & ( 1 << (4 - f))) {
																	inc += scnprintf( oneop+inc, sizeof(oneop)-inc, "%s%s",
																		numflags > 0 ? "|" : "", flags[f]
																	);
																	numflags++;
																}
															}
														}
														else {
															scnprintf( oneop, sizeof(oneop), "0" );
														}
													}
													
													else if ( insn->id == PPC_INS_RLWINM ) {
														if (
															ppc->op_count == 5 &&
															operands[2].type == PPC_OP_IMM &&
															operands[3].type == PPC_OP_IMM &&
															operands[4].type == PPC_OP_IMM
														) {
															int n, b;

															// rlwinm rA,rS,n,0,31               rotlwi rA,rS,n                // capstone handles this
															// rlwinm rA,rS,32 – n,0,31          rotrwi rA,rS,n
															// rlwinm rA,rS,0,n,31               clrlwi rA,rS,n (n < 32)       // capstone handles this

															// rlwinm rA,rS,n,0,31 – n           slwi rA,rS,n (n < 32)
															n = (int)operands[2].imm;
															if ( operands[3].imm == 0 && operands[4].imm == 31 - n ) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "slwi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																break;
															}

															// rlwinm rA,rS,32 – n,n,31          srwi rA,rS,n (n < 32)
															n = (int)operands[3].imm;
															if ( operands[2].imm == 32 - n && operands[4].imm == 31 && n < 32 ) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "srwi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																break;
															}

															// rlwinm rA,rS,0,0,31 – n           clrrwi rA,rS,n (n < 32)
															n = 31 - (int)operands[4].imm;
															if ( operands[2].imm == 0 && operands[3].imm == 0 ) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "clrrwi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																break;
															}

															// rlwinm rA,rS,b,0,n – 1            extlwi rA,rS,n,b (n > 0)
															b = (int)operands[2].imm;
															n = (int)operands[4].imm + 1;
															if ( operands[3].imm == 0 && n < 32 ) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "extlwi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																else if (i == 3) {
																	snprintf( oneop, sizeof(oneop), "%d", b );
																}
																break;
															}

															// rlwinm rA,rS,n,b – n,31 – n       clrlslwi rA,rS,b,n (n ≤ b < 32)
															n = (int)operands[2].imm;
															b = (int)operands[3].imm + n;
															if ( operands[4].imm == 31 - n && b < 32 ) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "clrlslwi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", b );
																}
																else if (i == 3) {
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																break;
															}

															// rlwinm rA,rS,b + n,32 – n,31      extrwi rA,rS,n,b (n > 0)
															n = 32 - (int)operands[3].imm;
															b = (int)operands[2].imm - n;
															if ( operands[4].imm == 31 && n < 32 && b >= 0 ) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "extrwi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																else if (i == 3) {
																	snprintf( oneop, sizeof(oneop), "%d", b );
																}
																break;
															}
														}
														snprintf( oneop, sizeof(oneop), "%lld", imm );
													}

													else if ( insn->id == PPC_INS_RLWIMI ) {
														if (
															ppc->op_count == 5 &&
															operands[2].type == PPC_OP_IMM &&
															operands[3].type == PPC_OP_IMM &&
															operands[4].type == PPC_OP_IMM
														) {
															int n, b;

															// rlwimi rA,rS,32 – b,b,b + n – 1             inslwi rA,rS,n,b
															b = (int)operands[3].imm;
															n = (int)operands[4].imm - b + 1;
															if ( operands[2].imm == 32 - b && n >= 0 && n < 32 ) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "inslwi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																else if (i == 3) {
																	snprintf( oneop, sizeof(oneop), "%d", b );
																}
																break;
															}

															// rlwimi rA,rS,32 – (b + n),b,(b + n) – 1     insrwi rA,rS,n,b (n > 0)
															b = (int)operands[3].imm;
															n = (int)operands[4].imm - b + 1;
															if ( operands[2].imm == 32 - (b + n) && n > 0 && n < 32 ) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "insrwi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																else if (i == 3) {
																	snprintf( oneop, sizeof(oneop), "%d", b );
																}
																break;
															}
														}
														snprintf( oneop, sizeof(oneop), "%lld", imm );
													}

													else if ( insn->id == PPC_INS_RLDIC ) {
														#if 0
														if (
															ppc->op_count == 4 &&
															operands[2].type == PPC_OP_IMM &&
															operands[3].type == PPC_OP_IMM
														) {
															int sh, me, n, b;
															sh = ((instruction >> 11) & 31) | (((instruction >> 1) & 1) << 5);
															me = ((instruction >>  6) & 31) | (((instruction >> 5) & 1) << 5);

															// rldic rA,rS,n,b – n              clrlsldi rA,rS,b,n (n ≤ b ≤ 63)

															// How is clrlsldi simpler than rldic? Or, when is rldic preferred?
															// For all values of sh and me, sh + me <= b + n, so there's no benefit.

															n = sh;
															b = me + n;
															if ( b < 64 ) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "clrlsldi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", b );
																}
																else if (i == 3) {
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																break;
															}
														}
														#endif
														snprintf( oneop, sizeof(oneop), "%lld", imm );
													}

													else if ( insn->id == PPC_INS_RLDICL ) {
														if (
															ppc->op_count == 4 &&
															operands[2].type == PPC_OP_IMM &&
															operands[3].type == PPC_OP_IMM
														) {
															int sh, me, n, b;
															sh = ((instruction >> 11) & 31) | (((instruction >> 1) & 1) << 5);
															me = ((instruction >>  6) & 31) | (((instruction >> 5) & 1) << 5);

															// rldicl rA,rS,n,0            rotldi rA,rS,n             // capstone handles this
															// rldicl rA,rS,0,n            clrldi rA,rS,n (n < 64)    // capstone handles this

															// rldicl rA,rS,64 – n,n       srdi rA,rS,n (n < 64)
															n = me;
															if ( sh == 64 - n ) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "srdi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																break;
															}

															// rldicl rA,rS,64 – n,0       rotrdi rA,rS,n
															n = 64 - sh;
															if ( me == 0 && n < 64 ) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "rotrdi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																break;
															}

															// rldicl rA,rS,b + n,64 – n   extrdi rA,rS,n,b (n > 0)
															n = 64 - me;
															b = sh - n;
															if ( sh > 0 && n < 64 && b >= 0 ) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "extrdi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																else if (i == 3) {
																	snprintf( oneop, sizeof(oneop), "%d", b );
																}
																break;
															}
														}
														snprintf( oneop, sizeof(oneop), "%lld", imm );
													}

													else if ( insn->id == PPC_INS_RLDICR ) {
														if (
															ppc->op_count == 4 &&
															operands[2].type == PPC_OP_IMM &&
															operands[3].type == PPC_OP_IMM
														) {
															int sh, me, n;
															sh = ((instruction >> 11) & 31) | (((instruction >> 1) & 1) << 5);
															me = ((instruction >>  6) & 31) | (((instruction >> 5) & 1) << 5);

															// rldicr rA,rS,n,63 – n         sldi rA,rS,n (n < 64)
															// also handled above (when insn->id == PPC_INS_SLDI)
															n = sh;
															if (me == 63 - n) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "sldi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																break;
															}
															
															// rldicr rA,rS,0,63 – n         clrrdi rA,rS,n (n < 64)
															n = 63 - me;
															if ( sh == 0 ) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "clrrdi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																break;
															}

															#if 0
															int b;
															
															// How is extldi simpler than rldicr? Or, when is rldicr preferred?
															// For all values of sh and me, sh + me <= b + n, so there's no benefit.

															// rldicr rA,rS,b,n – 1          extldi rA,rS,n,b (n > 0)
															b = sh;
															n = me + 1;
															if (n < 64) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "extldi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																else if (i == 3) {
																	snprintf( oneop, sizeof(oneop), "%d", b );
																}
																break;
															}
															#endif

														}
														snprintf( oneop, sizeof(oneop), "%lld", imm );
													}

													else if ( insn->id == PPC_INS_RLDIMI ) {
														if (
															ppc->op_count == 4 &&
															operands[2].type == PPC_OP_IMM &&
															operands[3].type == PPC_OP_IMM
														) {
															int sh, me, n, b;

															sh = ((instruction >> 11) & 31) | (((instruction >> 1) & 1) << 5);
															me = ((instruction >>  6) & 31) | (((instruction >> 5) & 1) << 5);

															// rldimi rA,rS,64 – (b + n),b     insrdi rA,rS,n,b (n > 0)
															b = me;
															n = 64 - b - sh;
															if ( n > 0
																&& ((b + n) < (sh + me)) // lower numbers are simpler
															) {
																if (i == 2) {
																	snprintf( mnemonic, sizeof(mnemonic), "insrdi%s", ppc->update_cr0 ? "." : "" );
																	snprintf( oneop, sizeof(oneop), "%d", n );
																}
																else if (i == 3) {
																	snprintf( oneop, sizeof(oneop), "%d", b );
																}
																break;
															}
														}
														snprintf( oneop, sizeof(oneop), "%lld", imm );
													}
													
													else {
														uint64_t num;
doimm:
														num = llabs( imm );
													
														if
															(
																	( num < 10 )
																||	( num % 10 == 0 && num <= 100 )
																||	( num % 100 == 0 && num <= 10000 )
																||	( num % 1000 == 0 )
															)
														{
															snprintf( oneop, sizeof(oneop), "%lld", imm );
														}
														else {
															if ( (uint32_t)((uint32_t)(imm >> 16) + 1) > 1 )
																snprintf( oneop, sizeof(oneop), "%s0x%llX", imm < 0 ? "-" : "", num );
															else
																snprintf( oneop, sizeof(oneop), "%s0x%04llX", imm < 0 ? "-" : "", num );
														}
													}
												} // case PPC_OP_IMM
												break;
												
												case PPC_OP_MEM:
													if ( ppc_op->mem.base == PPC_REG_R0 && ppc_op->mem.offset == PPC_REG_INVALID )
														snprintf( oneop, sizeof(oneop), "%d(0)", ppc_op->mem.disp );
													else if ( ppc_op->mem.base == PPC_REG_ZERO && ppc_op->mem.disp == 0 ) {
														if (ppc_op->mem.offset == PPC_REG_INVALID)
															snprintf( oneop, sizeof(oneop), "0(0)");
														else
															snprintf( oneop, sizeof(oneop), "0,%s", cs_reg_name( handle, ppc_op->mem.offset ) );
													}
													else if ( ppc_op->mem.offset == PPC_REG_INVALID )
														snprintf( oneop, sizeof(oneop), "%d(%s)", ppc_op->mem.disp, cs_reg_name( handle, ppc_op->mem.base ) );
													else if ( ppc_op->mem.disp == 0 )
														snprintf( oneop, sizeof(oneop), "%s,%s", cs_reg_name( handle, ppc_op->mem.base ), cs_reg_name( handle, ppc_op->mem.offset ) );
													else
														snprintf( oneop, sizeof(oneop), "%s,%s+%d", cs_reg_name( handle, ppc_op->mem.base ), cs_reg_name( handle, ppc_op->mem.offset ), ppc_op->mem.disp );
													break;

/*
												case PPC_OP_CRX:
													snprintf( oneop, sizeof(oneop), "%s_%s",
														// ppc_op->crx.scale,
														cs_reg_name( handle, ppc_op->crx.reg ),
														ppc_op->crx.cond == PPC_BC_INVALID ? "" :
														ppc_op->crx.cond == PPC_BC_LT      ? "LT" :
														ppc_op->crx.cond == PPC_BC_LE      ? "LE" :
														ppc_op->crx.cond == PPC_BC_EQ      ? "EQ" :
														ppc_op->crx.cond == PPC_BC_GE      ? "GE" :
														ppc_op->crx.cond == PPC_BC_GT      ? "GT" :
														ppc_op->crx.cond == PPC_BC_NE      ? "NE" :
														ppc_op->crx.cond == PPC_BC_UN      ? "UN" :
														ppc_op->crx.cond == PPC_BC_NU      ? "NU" :
														ppc_op->crx.cond == PPC_BC_SO      ? "SO" :
														ppc_op->crx.cond == PPC_BC_NS      ? "NS" :
														"?"
													);
													break;
*/

												default:
													fprintf( stderr, "Line %d # Assembly instruction has invalid operand\n", linenum + 1 );
													break;
											} // switch
										
											if (oneop[0]) {
												inc += scnprintf( operand+inc, operandsize-inc, "%s%s", out_op_count ? "," : "", oneop );
												out_op_count++;
											}
										
										} // for
									} // operands
								} // if gPass (for instructions that don't have more than one line)
								cs_free( insn, count );
								gotInstruction = TRUE;
							} // override operands
						} // if count && insn
						else { // handle unknown instructions
							// capstone doesn't output anything for this instruction
							
							if ( (instruction & 0xfc7fffff) == 0x7c000400 ) { // mcrxr
								snprintf( mnemonic, sizeof(mnemonic), "mcrxr" );
								snprintf( operand, sizeof(operand), "%s", cs_reg_name( handle, (( instruction >> 23 ) & 7) + PPC_REG_CR0 ) );
								gotInstruction = TRUE;
							}
							else if ( (instruction & 0xfc0003fe) == 0x7c000010 ) { // subfc (convert to subc)
								snprintf( mnemonic, sizeof(mnemonic), "subc%s%s", ((instruction >> 10) & 1) ? "o" : "", (instruction & 1) ? "." : "" );
								snprintf( operand, sizeof(operand), "r%d,r%d,r%d",
									(instruction >> 21) & 31,
									(instruction >> 11) & 31,
									(instruction >> 16) & 31
								);
								gotInstruction = TRUE;
							}
							else if ( (instruction & 0xfc0003fe) == 0x7c0000d0 ) { // neg
								snprintf( mnemonic, sizeof(mnemonic), "neg%s%s", ((instruction >> 10) & 1) ? "o" : "", (instruction & 1) ? "." : "" );
								snprintf( operand, sizeof(operand), "r%d,r%d",
									(instruction >> 21) & 31,
									(instruction >> 16) & 31
								);
								gotInstruction = TRUE;
							}
							else if ( (instruction & 0xfc0003fe) == 0x7c000194 ) { // addze
								snprintf( mnemonic, sizeof(mnemonic), "addze%s%s", ((instruction >> 10) & 1) ? "o" : "", (instruction & 1) ? "." : "" );
								snprintf( operand, sizeof(operand), "r%d,r%d",
									(instruction >> 21) & 31,
									(instruction >> 16) & 31
								);
								gotInstruction = TRUE;
							}
							else if ( (instruction & 0xfc0003fe) == 0x7c0001d0 ) { // subfme
								snprintf( mnemonic, sizeof(mnemonic), "subfme%s%s", ((instruction >> 10) & 1) ? "o" : "", (instruction & 1) ? "." : "" );
								snprintf( operand, sizeof(operand), "r%d,r%d",
									(instruction >> 21) & 31,
									(instruction >> 16) & 31
								);
								gotInstruction = TRUE;
							}
							else if ( (instruction & 0xfc0003fe) == 0x7c0001d4 ) { // addmex
								snprintf( mnemonic, sizeof(mnemonic), "addme%s%s", ((instruction >> 10) & 1) ? "o" : "", (instruction & 1) ? "." : "" );
								snprintf( operand, sizeof(operand), "r%d,r%d",
									(instruction >> 21) & 31,
									(instruction >> 16) & 31
								);
								gotInstruction = TRUE;
							}

							else if ( (instruction & 0xfc0007ff) == 0x7c0002a6 ) { // mfspr

							}

							else if ( (instruction & 0xfc0007ff) == 0x7c0003a6 ) { // mtspr

							}


							else if ( (instruction & 0xfc0007ff) == 0x7c00052a ) { // stswx
								snprintf( mnemonic, sizeof(mnemonic), "stswx" );
								snprintf( operand, sizeof(operand), "r%d,r%d,r%d",
									(instruction >> 21) & 31,
									(instruction >> 16) & 31,
									(instruction >> 11) & 31
								);
								gotInstruction = TRUE;
							}
							else if ( (instruction & 0xfc6007ff) == 0xfc000040 ) { // fcmpo
								snprintf( mnemonic, sizeof(mnemonic), "fcmpo" );
								snprintf( operand, sizeof(operand), "%s,fp%d,fp%d",
									cs_reg_name( handle, (( instruction >> 23 ) & 7) + PPC_REG_CR0 ),
									(instruction >> 16) & 31,
									(instruction >> 11) & 31
								);
								gotInstruction = TRUE;
							}
						} // handle unknown instructions
#else
						DisassemblerStatus disasmStat = Disassembler_OK ^ ppcDisassembler(
							&instructionMem,
							adjust + currentPos,
							options,
							mnemonic,
							operand,
							comment,
							NULL, /* refCon */
							MyDisassemblerLookups /* DisassemblerLookups lookupRoutine */
						);
						if ( instruction == 0x7C2002A6 ) /* this instruction "mfspr r1,MQ ; 0" has the Disassembler_InvField bit set because it is 601 specific */
						{
							disasmStat &= ~Disassembler_InvField;
							fprintf( stderr, "Line %d # Assembly instruction has invalid field (MQ is a PPC601 specific special purpose register)\n", linenum + 1 );
						}
						if ( ( disasmStat & Disassembler_InvSprMaybe ) != 0 )
						{
							if ( !gPass )
								fprintf( stderr, "Line %d # Possibly invalid special purpose register %ld\n", linenum + 1, gSpecialPurposeRegister );
						}
						if ( ( disasmStat & ( Disassembler_OK | Disassembler_InvRsvBits | Disassembler_InvField /*| Disassembler_Privileged*/ ) ) == 0 )
						{
							gotInstruction = TRUE;
						}
#endif
					} // if instruction != 0xDEADBEEF
					if ( !gotInstruction ) {
						set_streampos( currentPos );
						break; /* got bad instruction so break */
					}

					if ( gPass )
					{
						dump_line( currentPos );
						u8 bytes[4];
						dump_buffer_hex( storebe( instruction, bytes ), 4, 2 );
						make_buffer_text( instructionText, bytes, 4 );

						comment2 = "";
						size_t commentlen = strlen( comment );

						if ( instruction == 0x7C0006AC ) /* eieio */
							comment[0] = 0;
						else if ( strstr( comment, " = " ) )
							comment[0] = 0;
						else if ( commentlen >= 12 && commentlen < 20 ) // between 8 and 15 hex digits following "; 0x"
						{
							if ( memcmp( "; 0x00", comment, 6 ) == 0 )
							{
								comment[0] = 0;
								comment2 = comment + 6;
							}
							else if ( memcmp( "; 0x", comment, 4 ) == 0 )
							{
								comment[0] = 0;
								comment2 = comment + 4;
							}
						}

						if ( comment[0] )
							printf( "%12s          %-8s %-22s %-16s \"%s\"%s\n", comment2, mnemonic, operand, comment, instructionText, comment3 );
						else
							printf( "%12s          %-8s %-39s \"%s\"%s\n", comment2, mnemonic, operand, instructionText, comment3 );
					}
					else
						linenum++;

					currentPos += 4;

					if ( gDissasembledToken != NULL )
					{
						if ( currentPos < instructionEnd && !strcmp( mnemonic, "bl" ) )
						{
							u32 theNum;
							u32 tokenAddress;
							
							if ( gDissasembledTokenOffset == 0 ) {
								if ( !strcmp( gDissasembledToken->name, "{'}" ) )
								{
									theNum = get_num32(); /* bl tokenname instruction */
									tokenAddress = (((s32)theNum & 0x03FFFFFC) << 6) / 64 + currentPos + romstartoffset;
									dumptokenline( tokenAddress, theNum, currentPos );
									currentPos += 4;
								}
								else
								if ( !strcmp( gDissasembledToken->name, "b<\">" ) )
								{
									bool isGoodString;
									char* string;
									u8 len;

									string = get_string2( &len, &isGoodString );
									if ( string && isGoodString )
									{
										if ( gPass )
										{
											char strbuff[2000];
										
											pretty_string(string, len, strbuff, sizeof(strbuff));

											dump_line( currentPos );
											printf( "%02X ...    %12s          %-8s %s\n", len, "", "dc.b", strbuff );
										}
										else
											linenum++;

										currentPos += ((len + 1 + 3) & -4);
										if ( currentPos > instructionEnd )
										{
											currentPos = instructionEnd;
											if ( !gPass )
												fprintf( stderr, "Line %d # string litteral extends too far\n", linenum );
										}
									}
									free(string);
									set_streampos( currentPos );
								}
								else
								if (
									!strcmp( gDissasembledToken->name, "b<lit>" ) ||
									!strcmp( gDissasembledToken->name, "(val)" ) ||
									!strcmp( gDissasembledToken->name, "(var)" )
								)
								{
									theNum = get_num32();
									dumpliteralline( theNum, currentPos );
									currentPos += 4;
								}
								else
								if (
									!strcmp( gDissasembledToken->name, "(i-val)" ) ||
									!strcmp( gDissasembledToken->name, "(i-var)" ) ||
									!strcmp( gDissasembledToken->name, "(field)" )
								)
								{
									theNum = get_num32(); /* offset in instance or struct */
									dumpoffsetline( theNum, currentPos );
									currentPos += 4;
								}
								else
								if ( !strcmp( gDissasembledToken->name, "(v-val)" ) )
								{
									theNum = get_num32();
									dumpoffsetline( theNum, currentPos );
									currentPos += 4;
									theNum = get_num32();
									dumpliteralline( theNum, currentPos );
									currentPos += 4;
									theNum = get_num32();
									dumptokenline( theNum, theNum, currentPos );
									currentPos += 4;
								}
								else
								if ( !strcmp( gDissasembledToken->name, "(i-defer)" ) )
								{
									theNum = get_num32();
									dumpoffsetline( theNum, currentPos );
									currentPos += 4;
									theNum = get_num32();
									dumptokenline( theNum, theNum, currentPos );
									currentPos += 4;
								}
								else
								if ( !strcmp( gDissasembledToken->name, "(v-defer)" ) )
								{
									theNum = get_num32();
									dumpoffsetline( theNum, currentPos );
									currentPos += 4;
									theNum = get_num32();
									dumptokenline( theNum, theNum, currentPos );
									currentPos += 4;
									theNum = get_num32();
									dumptokenline( theNum, theNum, currentPos ); /* storage */
									currentPos += 4;
								}
								else
								if (
									!strcmp( gDissasembledToken->name, "b<to>"   ) ||
									!strcmp( gDissasembledToken->name, "b<'>"    ) ||
									!strcmp( gDissasembledToken->name, "(i-to)"  ) ||
									!strcmp( gDissasembledToken->name, "(v-to)"  ) ||
									!strcmp( gDissasembledToken->name, "(defer)" )
								)
								{
									theNum = get_num32();
									tokenAddress = theNum;
									dumptokenline( tokenAddress, theNum, currentPos );
									currentPos += 4;
								}
								else
								if ( !strcmp( gDissasembledToken->name, "b<to>1" ) )
								{
									theNum = get_num32();
									tokenAddress = theNum + currentPos + romstartoffset;
									dumptokenline( tokenAddress, theNum, currentPos );
									currentPos += 4;
								}
							} // if gDissasembledTokenOffset == 0
						} /* if ( currentPos < instructionEnd && !strcmp( mnemonic, "bl" ) ) */
					} /* if gDissasembledToken */
				} /* if gLastTokenIsAlias else */
			} /* while good instructions */
		} /* while */
	} /* if mac_rom */

#endif /* USEDISASSEMBLER */
	
	dump_hex( currentPos, endPos );

} /* disassemble_lines */


void mark_last_streampos(void)
{
	gLastPos = get_streampos();
}

/* Old World G3, 8600 flags also used by G5 */
										
										/* flag names defined in G5 Open Firmware */
										
#define tokenFlagDefined		0x80	/* fdefd		- token is completely defined and ready to be called */
#define tokenFlagImmediate		0x40	/* fimm			- tokens like instance, vectored, b(lit), b('), b("), b(<mark), b(<resolve), b(>mark), b(>resolve), bbranch, lbbranch, b(do), b(case), external-token, if, case, then, of, while, repeat, do, */
#define tokenFlagNoHeader       0x20	/* fnohdr		- token created with new-token (instead of external-token or named-token) while headerless (instead of external or headers) */
#define tokenFlagAlias			0x10	/* falias		- token created with alias */
#define tokenFlagInstance		0x08	/* finstance	- token created with instance */
/* G5 only flags */
#define tokenFlagVisible		0x04	/* fvisible		- token created with external-token (instead of named-token or new-token) while external (instead of headers or headerless) */
#define tokenFlagInvisible		0x02	/* finvisible	- token created with named-token (instead of external-token or new-token) while headers (instead of external or headerless) */
#define tokenFlagVectored		0x01	/* fvectored	- token created with vectored */


void decode_rom_token(void)
{
	u16 definedFCodeNumber;
	char* stringFromFile;
	char* stringForTokenName = NULL;
	char* stringToDelete = NULL;
	u8 len;
	u8 mac_rom_fcode_flags;
	bool good_token_name;
	u32 defined_token_pos, after_defined_token_pos, token_code_pos;
	token_t* theToken;

	token_t* aliasToken = NULL;
	u32 alias_token_offset = 0;
	u32 alias_token_pos = 0;

	int numZeros = 0;
	bool isGoodString;


/*
value
	b(value) [0x0b8] 0x4e5
	7E48 02A6                       mflr     r18                 
	4BFF C8C5                       bl       (val)               
	0000 0000                                                    
	
	b(code) [0x0bf] 0x432
	7E49 03A6                       mtctr    r18                 
	7C68 02A6                       mflr     r3                  
	969F FFFC                       stwu     r20,-0x0004(r31)    
	8283 0000                       lwz      r20,0x0000(r3)      
	4E80 0420                       bctr                         

buffer
	b(buffer:) [0x0bd] 0x4ea
	7E48 02A6                       mflr     r18                 
	4BFF C84D                       bl       (buffer)            
	0000 0100                                                    
	
	b(code) [0x0bf] 0x432
	7E49 03A6						mtctr    r18                 
	7C68 02A6						mflr     r3                  
	969F FFFC						stwu     r20,-0x0004(r31)    
	8283 0000						lwz      r20,0x0000(r3)      
	4E80 0420						bctr                         

variable
	b(variable) [0x0b9] 0x4e4
	7E48 02A6						mflr     r18                 
	4BFF C9BD						bl       (var)               
	0000 0000                                                    
	
	b(code) [0x0bf] 0x437
	7E49 03A6						mtctr    r18                 
	969F FFFC						stwu     r20,-0x0004(r31)    
	7E88 02A6						mflr     r20                 
	4E80 0420						bctr                         

constant
	b(constant) [0x0ba] 0x4ee
	969F FFFC						stwu     r20,-0x0004(r31)    
	3A80 0018						li       r20,0x0018          
	4E80 0020						blr                          
	
	b(constant) [0x0ba] 0x4fa
	969F FFFC						stwu     r20,-0x0004(r31)    
	3E80 0000						lis      r20,0x0000          
	6294 FFFF						ori      r20,r20,0xFFFF      
	4E80 0020						blr                          
	
	b(constant) [0x0ba] 0x4fd
	969F FFFC						stwu     r20,-0x0004(r31)    
	3E80 4800						lis      r20,0x4800          
	4E80 0020						blr                          

field
	b(field) [0x0be] 0x502
	3A94 0000						addi     r20,r20,0x0000      
	4E80 0020						blr                          
	FFFF FFF0                                                    

alias
	b(constant) [0x0ba] 0x4ff
	FFFF FF28                                                    
	b(constant) [0x0ba] 0x500
	FFFF FF48                                                    
	b(constant) [0x0ba] 0x501
	FFFF FF20                                                    

set-token
	set-token [0x0db] 0xe19 %r0
	967E FFFC                       stwu     r19,-4(r30)         
	7E68 02A6                       mflr     r19                 
	4BFF FEE9       00E0F8          bl       mac_rom_colon_70C+64

*/

/* Check token file pos */

	defined_token_pos = (u32)get_streampos();
	if ( (defined_token_pos & 3) != 2 )
		return;

/* Check token FCode number */

	definedFCodeNumber = get_num16();
	if (
		(
			(mac_rom & 0x24) == 0
			&& definedFCodeNumber > 0xFFF /* Macs before G4 don't define anything above 0xfff */
		) ||
		(
			(mac_rom & 0x20) != 0
			&& definedFCodeNumber > 0xFFF
			&& definedFCodeNumber < 0xFFFF /* G4 uses 0xffff for %r1 etc. */
		) ||
#if 0
		(
			(mac_rom & 4) != 0
			&& definedFCodeNumber > 0x3FFF /* the G5 has FCode's greater than 0xFFF */
			&& definedFCodeNumber < 0xC000 /* the G5 uses FCode 0xFFFF for user defined words */
		) ||
#endif
		(
			1
			&& (definedFCodeNumber != 0x000) /* defined by b(:) in mac rom */
			&& (definedFCodeNumber <= 0x00F) /* No Mac defines fcodes 0x001 to 0x00f */
		)
	)
	{
		set_streampos( defined_token_pos );
		return;
	}

/* Check token flags */

	set_streampos( current_token_pos - 1 );
	mac_rom_fcode_flags = get_num8();

	if	(
			( ( ( /* tokenFlagUnusedFlags | */ tokenFlagDefined) & mac_rom_fcode_flags ) != tokenFlagDefined )
		)
	{
		set_streampos( defined_token_pos );
		return;
	}

/* Get token name */

	after_defined_token_pos = defined_token_pos + 2;

	set_streampos( after_defined_token_pos );
	stringFromFile = get_string2( &len, &isGoodString );

	good_token_name = stringFromFile && isGoodString && good_tokenname( stringFromFile, len );
	if ( good_token_name != ( ( mac_rom_fcode_flags & tokenFlagNoHeader ) == 0 ) )
	{
		set_streampos( defined_token_pos );
		return;
	}

/* Find offset of token code */

	if ( good_token_name )
	{
		numZeros=0;
		while (get_num8()==0 && numZeros<256)
			numZeros++;
		token_code_pos = (u32)get_streampos() - 1; /* after the last zero */
	}
	else
		token_code_pos = after_defined_token_pos;

/* Check alias token */

	if ( mac_rom_fcode_flags & tokenFlagAlias )
	{
		set_streampos( token_code_pos );
		alias_token_pos = token_code_pos + get_num32();
		aliasToken = find_token_by_pos( alias_token_pos, &alias_token_offset );
		if ( !aliasToken )
		{
			set_streampos( defined_token_pos );
			return;
		}
	}

/* Check last token offset and dump everything since then */
	{
		u32 caclulated_previous_rom_token_pos;
		s32 previous_token_offset;

		set_streampos(current_token_pos - 5);
		
		previous_token_offset = get_num32();

		if ( previous_token_offset == 0 )
		{
			set_streampos(current_token_pos - 5);
			decode_lines();

			u8 bytes[4];
			dump_hex_one_line( storebe( previous_token_offset, bytes ), 4 );
			if ( gPass )
				fprintf(stderr, "Line %d # Offset %06X; Previous token offset is 0\n", linenum, current_token_pos - 5 + romstartoffset);
		}
		else
		{
			caclulated_previous_rom_token_pos = previous_token_offset + current_token_pos - 5;
		
			if ( caclulated_previous_rom_token_pos == gLastMacRomTokenPos )
			{
				set_streampos(current_token_pos - 5);
				decode_lines();
				dumptokenoffset( previous_token_offset, caclulated_previous_rom_token_pos + romstartoffset );
			}
			else if ( previous_token_offset < 0 && caclulated_previous_rom_token_pos >= 0 )
			{
				u32 insidetokenoffset;
				token_t * backtoken = find_token_by_hlinkpos( caclulated_previous_rom_token_pos, &insidetokenoffset);
				
				if ( backtoken != NULL && insidetokenoffset == 0 )
				{
					set_streampos(current_token_pos - 5);
					decode_lines();
					dumptokenoffset( previous_token_offset, caclulated_previous_rom_token_pos + romstartoffset );
					if ( !gPass )
						fprintf(stderr, "Line %d # Offset %06X; Token offset points to %s at %06X\n", linenum, current_token_pos - 5 + romstartoffset, backtoken->name, backtoken->execution_pos + romstartoffset );
				}
				else
				{
					if ( !gPass )
						fprintf(stderr, "Line %d # Offset %06X; Previous token offset %06X -> %06X doesn’t have a token\n", linenum, current_token_pos - 5 + romstartoffset, previous_token_offset, caclulated_previous_rom_token_pos + romstartoffset );
					set_streampos( defined_token_pos );
					return;
				}
			}
			else
			{
				/* offset is bad - don’t report anything */
				set_streampos( defined_token_pos );
				return;
			}

			if ( previous_token_offset & 7 )
				if ( !gPass )
					fprintf(stderr, "Line %d # Offset %06X; Previous token offset should be a multiple of 8\n", linenum, current_token_pos - 5 + romstartoffset);
		}
	}

/* All good; mark it */

	gLastMacRomTokenPos = current_token_pos - 5;

/* Dump token flags */

	dump_line(current_token_pos - 1);
	dump_hex_one_line( &mac_rom_fcode_flags, 1 );

/* Dump token type */

	dump_line(current_token_pos);
	output_token();

/* Check alias token */

	if ( aliasToken && alias_token_offset == 0 )
	{
		gLastTokenIsAlias = TRUE;
		set_streampos( alias_token_pos ); /* for token naming purposes; if it doesn't have a name */
	}
	else
		set_streampos( token_code_pos );

/* dump token name */

	if ( good_token_name )
	{
		if ( !gPass )
		{
			int width = numZeros + len + 1;
			if ( ( width != 8 && width != 16 && width != 24 && width != 32 ) || numZeros > 7 )
				fprintf( stderr, "Line %d # zeros:%d width:%d\n", linenum, numZeros, width );
			stringForTokenName = stringFromFile;
		}
	}
	else
	{
		{
			/* the name from the file was bad so clean it up if it has any length */
			int numleft;
			char* p;

			numleft = len;
			for (p=stringFromFile; numleft; numleft--,p++)
				if (*p==13 || *p==10 || *p==0)
					*p = ' ';
		}

		if ( !gPass )
		{
			const char *token_class;
			int new_token_len = 0;
			char new_token_name[50];
			const char* oldname;
			u32 instructions[5];
			s32 number_for_name = 0;
			bool number_is_good;

			if ( ( mac_rom_fcode_flags & tokenFlagNoHeader ) == 0 )
				fprintf(stderr, "Line %d # Bad %s token name (len:%d): \"%s\"\n", linenum, current_token_name, len, stringFromFile);

			theToken = find_token(definedFCodeNumber);
			oldname = get_token_name(theToken);

			if ( theToken == NULL || oldname == unnamed || definedFCodeNumber >= 0x600 )
			{
				/* make up a name based on token type and contents if the token was not defined before or was unnamed or is a user defined fcode */

				switch (current_token_fcode)
				{
					case 0x0bf: /* b(code) */
						token_class = "code";
						goto DoNoNumber;
						
					case 0x0b7: /* b(:) */
						token_class = "colon";
						goto DoNoNumber;
						
					case 0x0bb: /* b(create) */
						token_class = "create";
						goto DoNoNumber;
						
					case 0x0bc: /* b(defer) */
						token_class = "defer";
						goto DoNoNumber;

					case 0x0db: /* set-token */
						token_class = "settoken";
		DoNoNumber:
						new_token_len = snprintf( new_token_name, sizeof(new_token_name), "mac_rom_%s_%03X", token_class, definedFCodeNumber );
						break;

					case 0x0b8: /* b(value) */
						token_class = "value";
						goto DoInlineNumber;
						
					case 0x0b9: /* b(variable) */
						token_class = "variable";
						goto DoInlineNumber;

					case 0x0bd: /* b(buffer:) */
					{
						token_class = "buffer";
		DoInlineNumber:
						instructions[0] = get_num32();
		DoInlineNumber2:
						instructions[1] = get_num32();
						instructions[2] = get_num32();

						number_for_name = instructions[2];
						number_is_good = instructions[0] == 0x7E4802A6 && (instructions[1] & 0xFF000000) == 0x4B000000; /* FE000000 0x4A000000 */

		DoCheckNumber:
						if (number_is_good)
						{
							new_token_len = snprintf( new_token_name, sizeof(new_token_name), "mac_rom_%s_%03X_%X", token_class, definedFCodeNumber, number_for_name );
						}
						else
						{
							new_token_len = snprintf( new_token_name, sizeof(new_token_name), "mac_rom_%s_%03X", token_class, definedFCodeNumber );
							if (!number_is_good)
								fprintf(stderr, "Line %d # Bad %s definition for \"%s\"\n", linenum, current_token_name, new_token_name);
						}
						break;
					}
					case 0x0ba: /* b(constant) */
					{
						bool gotNum = FALSE;
						int index;
						number_is_good = FALSE;
						
						token_class = "constant";
						
						instructions[0] = get_num32();
						instructions[1] = get_num32();
						instructions[2] = get_num32();
						instructions[3] = get_num32();

						if ( instructions[0] == 0x969FFFFC )
						{
							number_for_name = 0;
							for ( index = 1; index < 4; index++ )
							{
								if ( instructions[index] == 0x4E800020 ) /* blr */
								{
									number_is_good = gotNum;
									break;
								}

								switch ( instructions[index] & 0xFFFF0000 )
								{
									case 0x3A800000: /* li */
										number_for_name = (s16)( instructions[index] & 0x0000FFFF );
										gotNum = TRUE;
										break;
									case 0x3E800000: /* lis */
										number_for_name = instructions[index] << 16;
										gotNum = TRUE;
										break;
									case 0x62940000: /* ori */
										number_for_name |= instructions[index] & 0x0000FFFF;
										break;
									default:
										goto DoCheckNumber;
								}
							}
						}
						goto DoCheckNumber;
					}

					case 0x0be: /* b(field) */
					{
						token_class = "field";

						instructions[0] = get_num32();
						if ( instructions[0] == 0x7E4802A6 )
							goto DoInlineNumber2;

						instructions[1] = get_num32();

						number_for_name = (s16)(instructions[0] & 0x0FFFF);
						number_is_good = (instructions[0] & 0xFFFF0000) == 0x3A940000 && instructions[1] == 0x4E800020;
						goto DoCheckNumber;
					}
					
				} /* switch */

				/* use new name */
				stringForTokenName = (char*)malloc( new_token_len + 1 );
				if (stringForTokenName)
					strcpy( stringForTokenName, new_token_name );
				else
				{
					fprintf( stderr, "Line %d # Not enough memory\n", linenum );
					exit(-ENOMEM);
				}
			}
			else
			{
				/* force use the old name if the token was defined and not unnamed and was not a user defined fcode */
				stringForTokenName = NULL;
			}

			stringToDelete = stringFromFile;
		} /* if ( !gPass ) */
	}

/* define token */
	if ( !gPass )
	{
		theToken = add_token( definedFCodeNumber, stringForTokenName );
		theToken->execution_pos = token_code_pos;
		theToken->hlink_pos = gLastMacRomTokenPos;
		if ( stringToDelete )
			free( stringToDelete );
	}
	else
	{
		u32 token_pos;
		theToken = find_token_by_pos( token_code_pos, &token_pos );
		if ( !theToken || token_pos )
		{
			/* this should never happen */
			theToken = NULL;
			fprintf(stderr, "Line %d # Could not find token for offset 0x%06X\n", linenum, token_code_pos );
			printf("0x%03x \\ token not found\n", definedFCodeNumber);
		}
		else if ( good_token_name )
			printf( "0x%03x %s\n", definedFCodeNumber, theToken->name );
		else if (len && len <= 32) /* had length but was bad */
			printf( "0x%03x %s \\ «%s» bad name, unnamed\n", definedFCodeNumber, stringFromFile, theToken->name );
		else
			printf( "0x%03x \\ %s\n", definedFCodeNumber, theToken->name );

	
/* dump special token info */
		if ( theToken )
		{
			if ( mac_rom_fcode_flags & ( tokenFlagNoHeader | tokenFlagVisible | tokenFlagInvisible ) )
				fprintf(stderr, "Line %d # Defined %s%s%stoken 0x%03x \"%s\"\n", linenum, 
					(mac_rom_fcode_flags & tokenFlagNoHeader) ? "headerless " : "named ",
					(mac_rom_fcode_flags & tokenFlagInvisible) ? "invisible " : "",
					(mac_rom_fcode_flags & tokenFlagVisible) ? "visible " : "",
					definedFCodeNumber, theToken->name);

			if ( mac_rom_fcode_flags & tokenFlagImmediate )
				fprintf(stderr, "Line %d # Defined immediate token 0x%03x \"%s\"\n", linenum, definedFCodeNumber, theToken->name );

			if ( mac_rom_fcode_flags & tokenFlagInstance )
				fprintf(stderr, "Line %d # Defined instance token 0x%03x \"%s\"\n", linenum, definedFCodeNumber, theToken->name);

			if ( mac_rom_fcode_flags & tokenFlagVectored )
				fprintf(stderr, "Line %d # Defined vectored token 0x%03x \"%s\"\n", linenum, definedFCodeNumber, theToken->name);

			if ( mac_rom_fcode_flags & tokenFlagAlias )
				fprintf(stderr, "Line %d # Defined alias token 0x%03x \"%s\" to 0x%03x \"%s\"\n", linenum, definedFCodeNumber, theToken->name, aliasToken->fcode, aliasToken->name );
		}
		else
			printf( "0x%03x\n", definedFCodeNumber );
	}

	set_streampos( token_code_pos );

	mark_last_streampos();
}


void code_s(void)
{
	u32 len = get_num32();

	if ( gPass ) {
		output_token();
		printf("code<<< ");
	}
	while (len >= 4) {
		u32 instruction = get_num32();
		if ( gPass ) {
			printf("%08x ", instruction);
		}
		len -= 4;
	}
	if ( gPass ) {
		printf(">>>\n");
	}
}


static void got_special_start(void) {
	indent=0;
	offs16 = FALSE;
	gStartPos=current_token_pos;
	got_start = TRUE;
	isSpecialStart = TRUE;
	decode_lines();
	if ( gPass )
		printf ("\\ detokenizing start at offset %06X\n", current_token_pos + 1 + romstartoffset);
	mark_last_streampos();
	if ( !gPass )
		fprintf(stderr, "Line %d # Unknown start\n", linenum);
}


void decode_rom_token2(void)
{
	u32 thepos = current_token_pos + 1;
	if (thepos >= 8)
	if ( ( thepos & 3 ) == 0 )
	if (thepos - gLastPos <= 32)
	{
		set_streampos( thepos - 8 );
		u32 num1 = get_num32(); // -8
		u32 num2 = get_num32(); // -4
		u32 num3 = get_num32(); // 0
		u32 num4 = get_num32(); // 4
		set_streampos( thepos );
		if ( (num1 & 0xfc000001) != 0x48000001 && num2 == 0x00000010 && num3 != 0 && num3 != 0xffffffff && num4 != 0xffffffff )
		{
			got_special_start();
		}
	}
}


void decode_rom_token203(void)
{
	u32 thepos = current_token_pos + 1;
	if (thepos >= 8)
	if ( ( thepos & 3 ) == 0 )
	if (gLastFcodeImageEndPos)
	if ((thepos - gLastFcodeImageEndPos) <= 32)
	if ((thepos - gLastFcodeImageEndPos) >= 16)
	{
		set_streampos( thepos - 8 );
		u32 num1 = get_num32(); // -8
		u32 num2 = get_num32(); // -4
		u32 num3 = get_num32(); // 0
		u32 num4 = get_num32(); // 4
		set_streampos( thepos );
		if ( (num1 & 0xfc000001) != 0x48000001 && num1 != 0 && num2 == 0 && num3 != 0 && num3 != 0xffffffff && num4 != 0xffffffff )
		{
			got_special_start();
		}
	}
}


void decode_rom_token3(void)
{
	u32 thepos = current_token_pos + 1;
	if (thepos >= 8)
	if ( ( thepos & 3 ) == 0 )
	if (gLastFcodeImageEndPos)
	if ((thepos - gLastFcodeImageEndPos) <= 88)
	if ((thepos - gLastFcodeImageEndPos) >= 16)
	{
		set_streampos( thepos - 8 );
		u32 num1 = get_num32(); // -8
		u32 num2 = get_num32(); // -4
		u32 num3 = get_num32(); // 0
		u32 num4 = get_num32(); // 4
		set_streampos( thepos );
		if ( (num1 & 0xfc000001) != 0x48000001 && num2 == 0 && num3 != 0 && num3 != 0xffffffff && num4 != 0xffffffff )
		{
			got_special_start();
		}
	}
}


void local_variables_declaration(void)
{
	/* local variables { local_0 ... local_n-1 ; ... } */
	int num=get_num8();
	if ( gPass )
	{
		int i;
		printf( "{ " );
		for ( i=0; i<num; i++ )
			printf( "local_%d ", i );
		printf( "; ... }" );
	}
	decode_default_of(TRUE);
}
