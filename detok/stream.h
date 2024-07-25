/*
 *                     OpenBIOS - free your system! 
 *                            ( detokenizer )
 *                          
 *  stream.h - prototypes for fcode streaming functions. 
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

#ifndef __STREAM__
#define __STREAM__

extern u16 fcode;

int init_stream(char *name);
int close_stream(void);

u32 get_streampos(void);
void set_streampos(u32 pos);

void get_mem(u8* dst, int size);
u16 get_token(void);
u8  get_num8(void);
u16 get_num16(void);
u32 get_num32(void);
s32 get_offset(bool islong);
char *get_string(u8 * len);
char *get_string2(u8 * len, bool* isGoodString);

#endif
