ARCH?=$(shell uname -m)
CC?=gcc
MAKEDEPEND?=$(CC) -M -MT$(OBJDIR)/$.o $(CFLAGS) $(INCS) -o $(DEPDIR)/$*.d $<

INSTALL_DIR?=/usr/local/bin
ARDOUR_MAPS_DIR?=/usr/share/ardour5/midi_maps

OFILES=$(CFILES:.c=.o)
DFILES=$(CFILES:.c=.d)

OBJDIR:=$(ARCH)/obj
DEPDIR:=$(ARCH)/dep
BINDIR:=$(ARCH)/bin

OBJECTS=$(addprefix $(OBJDIR)/, $(OFILES))
DEPS=$(addprefix $(DEPDIR)/, $(DFILES))

VPATH=src
CFILES=cs10-linux.c
INCS=-Iinclude
LIBS=-lasound

all: $(BINDIR)/cs10-linux

clean:
	@echo Cleaning $(ARCH)
	@rm -f $(OBJECTS) $(DEPS)

distclean: 
	@echo Distclean $(ARCH)
	@rm -rf *~ $(ARCH)

install:
	@echo Installing
	@cp $(BINDIR)/cs10-linux $(INSTALL_DIR)
	@cp midi_maps/* $(ARDOUR_MAPS_DIR)

$(ARCH)/.dirs:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(DEPDIR)
	@mkdir -p $(BINDIR)
	@touch $@

$(BINDIR)/cs10-linux: $(ARCH)/.dirs $(OBJECTS)
	@echo Linking $@
	@$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

$(OBJDIR)/%.o: %.c
	@echo Compiling $<
	@$(MAKEDEPEND)
	@$(CC) -c $(CFLAGS) $(INCS) -o $@ $<

.PHONY: all clean distclean install

-include $(DEPS)
