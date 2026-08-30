# Choose compiler (currently only gcc) using FLAVOUR= when
# invoking make or get the default of gcc
FLAVOUR ?= gcc
VARIANT = $(FLAVOUR)

SRCDIR = src
OBJDIR = obj_$(FLAVOUR)
LIBDIR = lib
INCDIR = include
EXTINC = ../extension/include

# Support for multiple compiler flags

ifeq (gcc,$(findstring gcc,$(FLAVOUR)))
   CC = gcc
   CFLAGS_ALL  = -I$(INCDIR) -I$(SRCDIR) -I$(EXTINC) -fPIC -g
   CFLAGS_OPT  = -O3
   CFLAGS_DBG  = -g -O0 -DSDF_DEBUG_ALL -D_XOPEN_SOURCE=600
   CFLAGS_DBG += -Wall -Wextra -Wno-unused-function -pedantic
   CFLAGS_DBG += -Wno-unused-parameter -std=c99
   CFLAGS_PROF = -p
endif

# utils
ECHO  = echo
RM    = rm
MKDIR = mkdir
CP    = cp

# compiler & archiver
AR  = ar
RANLIB = ranlib

# default mode (max. optimization)
mode = opt

CFLAGS = $(CFLAGS_ALL)

# add flags for debugging if requested
ifeq (dbg,$(findstring dbg,$(mode)))
   CFLAGS += $(CFLAGS_DBG)
endif

# add flags for profiling if requested
ifeq (pro,$(findstring pro,$(mode)))
   CFLAGS += $(CFLAGS_PROF)
endif

# add flags for optimization if requested
ifeq (opt,$(findstring opt,$(mode)))
   CFLAGS += $(CFLAGS_OPT)
endif

# objectlist file
include Makefile-objs

# target name
LIB = $(LIBDIR)/libsdfc.a

VPATH = $(SRCDIR):$(OBJDIR):$(LIBDIR):$(INCDIR)

# target
all: $(LIB)

FORCE:

# Not real file targets
.PHONY: Makefile Makefile-objs all clean cleanall help FORCE

.SUFFIXES: .o .c .h

# implicit rules
%.o: %.c
	$(CC) -c $(CFLAGS) -o $(OBJDIR)/$@ $<

commit_info.h: FORCE
	@cd $(SRCDIR) && sh gen_commit_string.sh . || true

$(LIB): $(OBJS)
	$(RM) -f $@
	$(AR) -rsu $@ $(addprefix $(OBJDIR)/,$(OBJS))
	$(RANLIB) $@

$(LIB2): $(LIB)
	$(CP) $(LIB) $(LIB2)

$(OBJS): | $(OBJDIR)

$(OBJDIR):
	$(MKDIR) -p $(OBJDIR)

$(LIB): | $(LIBDIR)

$(LIBDIR):
	$(MKDIR) -p $(LIBDIR)

# cleanup
clean:
	$(RM) -rf $(OBJDIR)

cleanall:
	$(RM) -rf obj_* $(LIBDIR) $(SRCDIR)/commit_info.h

# help page
help:
	@$(ECHO) "Defined targets:"
	@$(ECHO) "  all    : build targets (default)"
	@$(ECHO) "  clean  : cleanup"
	@$(ECHO) "Defined modes:"
	@$(ECHO) "  opt: enable flags for optimization (default)"
	@$(ECHO) "  dbg: enable flags for debugging"
	@$(ECHO) "  pro: enable flags for profiling"
	@$(ECHO) "Example:"
	@$(ECHO) "  type \`make mode=dbg+pro' to enable dbg and pro flags"

# dependencies file
#include Makefile-deps

sdf_control.o: commit_info.h
