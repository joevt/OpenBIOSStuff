#ifndef __MACROM__
#define __MACROM__

#include "detok.h"

void make_buffer_text(char *dstBuffer, u8 *srcBuffer, int len);
void disassemble_lines(u32 endPos, u32 currentPos);
void mark_last_streampos(void);
void code_s(void);
void decode_rom_token(void);
void decode_rom_token1122(void);
void decode_rom_token2(void);
void decode_rom_token203(void);
void decode_rom_token3(void);
void local_variables_declaration(void);
int scnprintf(char * str, size_t size, const char * format, ...) __printflike(3, 4);

#endif
