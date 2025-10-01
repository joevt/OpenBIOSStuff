#ifndef __DECODE__
#define __DECODE__

#include "detok.h"

extern int use_tabs, verbose, decode_all, ignore_len, mac_rom, linenumbers;
extern u32 current_token_pos;
extern u16 current_token_fcode;
extern s64 romstartoffset;
extern u32 addressmask;
extern u32 gStartPos, gLastPos, gLastFcodeImageEndPos;
extern int gPass, gLastMacRomTokenPos;
extern bool got_start, isSpecialStart;

extern bool debugcapstone;

extern int indent;
extern int linenum;

extern bool offs16;

extern u32 fclen;
extern size_t filelen;

extern const char * current_token_name;

int detokenize(void);
void dump_line(u32 pos);
void pretty_string(char *string, u8 len, char* strbuff, size_t sizebuff);
void decode_lines(void);
bool good_tokenname(char* tokenname, u8 len);
void output_token(void);
void decode_default_of(bool of);

#endif
