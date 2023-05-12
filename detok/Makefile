#
#                     OpenBIOS - free your system! 
#                            ( detokenizer )
#                          
#  This program is part of a free implementation of the IEEE 1275-1994 
#  Standard for Boot (Initialization Configuration) Firmware.
#
#  Copyright (C) 2001-2003 Stefan Reinauer, <stepan@openbios.org>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; version 2 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

ARCH     := $(shell uname -m | sed -e s/i.86/x86/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/x86_64/amd64/ -e "s/Power Macintosh/ppc/")
TOPDIR   := $(shell /bin/pwd)
BUILDDIR ?= $(TOPDIR)/obj-$(ARCH)

VPATH    := $(BUILDDIR)


include $(TOPDIR)/Rules.make


CC	  =  gcc
CFLAGS	  =  -O3 -Wall -DUSEDISASSEMBLER -DUSECAPSTONE -DLONGOPT #-ansi

# For debugging the indentation code of detok, define DEBUG_INDENT
#CFLAGS	 :=  $(CFLAGS) -DDEBUG_INDENT



# capstone library name (without prefix 'lib' and suffix '.so')
LIBNAME = capstone

all: main detok
	@echo "\nOpenBIOS detokenizer detok build finished\n"

main:
	@echo "Welcome to the OpenBIOS detokenizer.."
	@test -r $(BUILDDIR) || ( mkdir -p $(BUILDDIR); \
	  echo "\nCreating build directory $(BUILDDIR)" )
		
detok: detok.o dictionary.o decode.o stream.o stack.o macrom.o
	@echo "\nLinking fcode detokenizer detok..."
	@cd $(BUILDDIR) && ( $(CC) $(CFLAGS) $^ -l$(LIBNAME) -o $@; strip detok )
	@echo " ok"
	
clean:
	@test ! -d $(BUILDDIR) && \
		echo "Architecture $(ARCH) is already clean." || \
		( 						 \
			echo "Cleaning up architecture $(ARCH)"; \
			rm -rf $(BUILDDIR)			 \
			rm forth.dict.core			 \
		)

distclean: clean
	rm -f detok

detok.o:      detok.h stream.h detok.cpp
stream.o:     detok.h stream.h stream.cpp
decode.o:     detok.h stream.h decode.h macrom.h decode.cpp Makefile
macrom.o:     detok.h stream.h decode.h macrom.h macrom.cpp Makefile
dictionary.o: detok.h dictionary.h dictionary.cpp
stack.o:      detok.h stack.h stack.cpp
