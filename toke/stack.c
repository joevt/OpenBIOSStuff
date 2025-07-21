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


static stackitem *dstack,*startdstack,*enddstack;

/* internal stack functions */

int init_stack(void)
{
	startdstack=enddstack=(stackitem*)malloc(MAX_ELEMENTS*sizeof(stackitem));
	enddstack+=MAX_ELEMENTS;
	dstack=enddstack;
	return 0;
}

void clear_stack(void)
{
	if (dstack != enddstack) {
		printf (FILE_POSITION "clear_stack: stack has %ld items\n",
			iname, lineno, enddstack - dstack);
		dstack = enddstack;
	}
}

static void stackerror(int stat)
{
	printf (FILE_POSITION "FATAL: stack %sflow\n",
		iname, lineno,
		(stat)?"under":"over" );
	exit(-1);
}


void dpushtype(long type, size_t data)
{
#ifdef DEBUG_DSTACK
	printf("dpush: sp=%p, data=0x%lx, ",dstack, data);
#endif
	if (dstack<=startdstack)
		stackerror(0);
	else
		--dstack;
	dstack->type = type;
	dstack->data = data;
}


size_t dpoptype(long type)
{
	return dpoptypes( type, -1);
}


stackitem *dstackfindtype(long type)
{
	return dstackfindtypes(type, -1);
}

stackitem *dstackfindtypes(long type1, long type2)
{
	bool isNotFirst = FALSE;
	stackitem * curstack = dstack;
	while (curstack <= enddstack)
	{
		if (curstack->type == type1 || curstack->type == type2)
		{
			if ( isNotFirst )
				printf (FILE_POSITION "WARNING: stack item possibly popped out of order\n",
					iname, lineno);
			return curstack;
		}
		curstack++;
		isNotFirst = TRUE;
	}
	return NULL;
}

size_t dpoptypes(long type1, long type2)
{
	size_t val = 0;
#ifdef DEBUG_DSTACK
	printf("dpop: sp=%p, data=0x%lx, ", dstack, dstack->data);
#endif
	stackitem * curstack = dstackfindtypes(type1, type2);
	if (curstack)
	{
		val = curstack->data;
		memmove( dstack + 1, dstack, (unsigned long)curstack - (unsigned long)dstack );
		dstack++;
		return val;
	}

	if (dstack>=enddstack)
		stackerror(1);
	else {
		printf (FILE_POSITION "WARNING: Expecting pop %ld|%ld but got %ld\n",
			iname, lineno, type1, type2, dstack->type);
		val=(dstack++)->data;
	}
	return val;
}
