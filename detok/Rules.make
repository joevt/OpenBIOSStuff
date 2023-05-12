# tag: Makefile rules

VPATH    := $(VPATH):.

.S.o:
	@printf "  assembling $<..."
	@$(CC) -c -nostdlib $(INCLUDES) $(CFLAGS) $< -o $(BUILDDIR)/$@ && \
		echo " ok" || \
		echo " failed"

.c.o:
.cpp.o:
	@printf "  compiling $<..."
	@$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $(BUILDDIR)/$@ && \
		echo " ok" || \
		echo " failed"
