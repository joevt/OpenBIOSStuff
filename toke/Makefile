#
#                     OpenBIOS - free your system! 
#                            ( tokenizer )
#                          

ARCH     := $(shell uname -m | sed -e s/i.86/x86/ -e s/sun4u/sparc64/ -e s/arm.*/arm/ -e s/sa110/arm/ -e s/x86_64/amd64/ -e "s/Power Macintosh/ppc/")
TOPDIR   := $(shell /bin/pwd)
BUILDDIR ?= $(TOPDIR)/obj-$(ARCH)

VPATH    := $(BUILDDIR)


include $(TOPDIR)/Rules.make


CC	  =  gcc
CFLAGS	  =  -O2 -Wall #-ansi

# For debugging the indentation code of toke, define DEBUG_INDENT
#CFLAGS	 :=  $(CFLAGS) -DDEBUG_INDENT

all: main toke
	@echo "\nOpenBIOS tokenizer toke build finished\n"

main:
	@echo "Welcome to the OpenBIOS tokenizer.."
	@test -r $(BUILDDIR) || ( mkdir -p $(BUILDDIR); \
	  echo "\nCreating build directory $(BUILDDIR)" )
		
toke: toke.o dictionary.o emit.o macros.o scanner.o stack.o stream.o 
	@printf "\nLinking fcode tokenizer toke..."
	@cd $(BUILDDIR) && ( $(CC) $(CFLAGS) $^ -o $@; strip toke )
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
	rm -f toke

toke.o:       toke.h stream.h stack.h emit.h toke.c
dictionary.o: toke.h dictionary.h dictionary.c
emit.o:       toke.h stack.h emit.h emit.c
macros.o:     toke.h macros.c
scanner.o:    toke.h stack.h stream.h emit.h dictionary.h scanner.c
stack.o:      toke.h stack.h stream.h stack.c
stream.o:     toke.h stream.h stream.c
