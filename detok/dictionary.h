#ifndef __DICTIONARY__
#define __DICTIONARY__


#include "detok.h"



/*
System-defined FCode numbers:	0x000 ... 0x7FF
Historical FCode numbers:		0x000 ... 0x2FF - subset of these fcodes
Defined FCode numbers:			0x000 ... 0x5FF - subset of these fcodes defined in the standard

Reserved FCode numbers:			0x010 ... 0x5FF - subset of these fcodes not defined in the standard
Vendor-unique FCode numbers:	0x600 ... 0x7FF - reserved for vendor-specific use with built-in devices
Program-defined FCode numbers:	0x800 ... 0xFFF - private to a particular FCode program

Power Mac G5 FCode numbers:		0x1000 ... ????	- The Power Mac G5 Quad uses out of range tokens
								???? ... 0xFFFF	- The Power Mac G5 Quad uses 0xFFFF for user defined words
*/



typedef struct token {
	const char *name;
	const char *ofname;
	u16 fcode;
	u16 flags;
	s32 execution_pos;
	s32 hlink_pos;
	struct token *next;
} token_t;


/* flags */
#define kFCodeDefined				0x8000

#define kFCodeTypeMask				0x000F /* possible types 0..15 */
	#define kFCodeHistorical		0
	#define kFCodeNew				1
	#define kFCodeReserved			3
	#define kFCodeVendorUnique		4
	#define kFCodeProgramDefined	5
	#define kFCodeExtendedRange		6
	#define kFCodeNegativeRange		7

	#define kIgnoreToken			0x10
	#define kFCodePredefined		0x20
	#define kTokenFinished			0x40
	#define kPass1					0x80
	#define kPass2					0x100
	#define kLoggedHidden			0x200

/* globals */
extern const char* fcerror;
extern const char* unnamed;
extern token_t *dictionary;

/* prototypes */
token_t *find_token(u16 number, bool check_name);
const char *get_token_name(token_t *theToken);
const char *lookup_token(u16 number, bool check_name);
token_t *add_token(u16 number, const char *name);
void init_dictionary(void);
void dump_defined(void);
void clear_program_tokens(void);

#endif
