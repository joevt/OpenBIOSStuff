/*
 *                     OpenBIOS - free your system! 
 *                         ( FCode tokenizer )
 *                          
 *  stack.c - data and return stack handling for fcode tokenizer.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "toke.h"
#include "stack.h"
#include "stream.h"


#define GUARD_STACK
#define EXIT_STACKERR

#ifdef GLOBALSTACK
#define STATIC static
#else
#define STATIC
#endif
STATIC stackitem *dstack,*startdstack,*enddstack;
#undef STATIC

/* internal stack functions */

int init_stack(void)
{
	startdstack=enddstack=malloc(MAX_ELEMENTS*sizeof(stackitem));
	enddstack+=MAX_ELEMENTS;
	dstack=enddstack;
	return 0;
}

#ifdef GLOBALSTACK 

#ifdef GUARD_STACK
static void stackerror(int stat)
{
	printf (FILE_POSITION "FATAL: stack %sflow\n",
		iname, lineno,
		(stat)?"under":"over" );
#ifdef EXIT_STACKERR
	exit(-1);
#endif
}
#endif

#endif


#if 0

static void dpush(long data)
{
#ifdef DEBUG_DSTACK
	printf("dpush: sp=%p, data=0x%lx, ", dstack, data);
#endif
	--dstack;
	dstack->type = 0;
	dstack->data = data;
#ifdef GUARD_STACK
	if (dstack<startdstack) stackerror(0);
#endif
}

static long dpop(void)
{
	long val;
#ifdef DEBUG_DSTACK
	printf("dpop: sp=%p, data=0x%lx, ", dstack, dstack->data);
#endif
	val=(dstack++)->data;
#ifdef GUARD_STACK
	if (dstack>enddstack) stackerror(1);
#endif
	return val;
}

static long dget(void)
{
	return dstack->data;
}

#endif


void dpushtype(long type, long data)
{
#ifdef DEBUG_DSTACK
	printf("dpush: sp=%p, data=0x%lx, ",dstack, data);
#endif
	--dstack;
	dstack->type = type;
	dstack->data = data;
#ifdef GUARD_STACK
	if (dstack<startdstack) stackerror(0);
#endif
}


long dpoptype(long type)
{
	return dpoptypes( type, -1);
}


long dpoptypes(long type1, long type2)
{
	long val;
	bool isNotFirst = FALSE;
#ifdef DEBUG_DSTACK
	printf("dpop: sp=%p, data=0x%lx, ", dstack, dstack->data);
#endif
	stackitem * curstack = dstack;
	while (curstack <= enddstack)
	{
		if (curstack->type == type1 || curstack->type == type2)
		{
			val = curstack->data;
			memmove( dstack + 1, dstack, (unsigned long)curstack - (unsigned long)dstack );
			dstack++;
			if ( isNotFirst )
				printf (FILE_POSITION "WARNING: stack item possibly popped out of order\n",
					iname, lineno);
			return val;
		}
		curstack++;
		isNotFirst = TRUE;
	}
	
	val=(dstack++)->data;
#ifdef GUARD_STACK
	if (dstack>enddstack) stackerror(1);
#endif
	return val;
}
