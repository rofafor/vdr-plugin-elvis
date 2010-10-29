#
# Makefile for Elvis plugin
#

# Debugging on/off
#ELVIS_DEBUG = 1

# Use static json-c library
#ELVIS_LIBJSONC = 0.9

# Strip debug symbols?  Set eg. to /bin/true if not
STRIP = strip

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.
# IMPORTANT: the presence of this macro is important for the Make.config
# file. So it must be defined, even if it is not used here!
#
PLUGIN = elvis

### The version number of this plugin (taken from the main source file):

VERSION = $(shell grep 'const char VERSION\[\] *=' $(PLUGIN).c | awk '{ print $$5 }' | sed -e 's/[";]//g')

### The C++ compiler and options:

CXX      ?= g++
CXXFLAGS ?= -fPIC -g -O2 -Wall -Wextra -Wswitch-default -Wfloat-equal -Wundef -Wpointer-arith -Wconversion -Wcast-align -Wredundant-decls -Wno-unused-parameter -Woverloaded-virtual -Wno-parentheses

### The directory environment:

VDRDIR = ../../..
LIBDIR = ../../lib
TMPDIR = /tmp

### Libraries

LIBS = $(shell curl-config --libs)

ifndef ELVIS_LIBJSONC
LIBS += $(shell pkg-config --libs json)
endif

### Make sure that necessary options are included:

-include $(VDRDIR)/Make.global

### Allow user defined options to overwrite defaults:

-include $(VDRDIR)/Make.config

### The version number of VDR's plugin API (taken from VDR's "config.h"):

APIVERSION = $(shell sed -ne '/define APIVERSION/s/^.*"\(.*\)".*$$/\1/p' $(VDRDIR)/config.h)

### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### Includes and Defines (add further entries here):

INCLUDES += -I$(VDRDIR)/include

DEFINES += -D_GNU_SOURCE -DPLUGIN_NAME_I18N='"$(PLUGIN)"'

ifdef ELVIS_DEBUG
DEFINES += -DDEBUG
endif

### The object files (add further files here):

OBJS = common.o config.o events.o fetch.o menu.o player.o recordings.o resume.o searchtimers.o timers.o elvis.o widget.o

### The main target:

all: libvdr-$(PLUGIN).so i18n

### Static json-c library target:

json-c-$(ELVIS_LIBJSONC).tar.gz:
	@wget http://oss.metaparadigm.com/json-c/json-c-$(ELVIS_LIBJSONC).tar.gz

json: json-c-$(ELVIS_LIBJSONC).tar.gz
	@tar xzf json-c-$(ELVIS_LIBJSONC).tar.gz
	@-rm -rf json
	@mv json-c-$(ELVIS_LIBJSONC) json

json/.libs/libjson.a: json
	@cd json && ./configure --enable-static --disable-shared --with-pic
	$(MAKE) -C json all

ifdef ELVIS_LIBJSONC
JSONLIB = json/.libs/libjson.a
INCLUDES += -I.
endif

### Implicit rules:

%.o: %.c Makefile
	$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) $<

### Dependencies:

MAKEDEP = $(CXX) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

-include $(DEPFILE)

### Internationalization (I18N):

PODIR     = po
LOCALEDIR = $(VDRDIR)/locale
I18Npo    = $(wildcard $(PODIR)/*.po)
I18Nmsgs  = $(addprefix $(LOCALEDIR)/, $(addsuffix /LC_MESSAGES/vdr-$(PLUGIN).mo, $(notdir $(foreach file, $(I18Npo), $(basename $(file))))))
I18Npot   = $(PODIR)/$(PLUGIN).pot

%.mo: %.po
	msgfmt -c -o $@ $<

$(I18Npot): $(wildcard *.c)
	xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP --msgid-bugs-address='<see README>' -o $@ $^

%.po: $(I18Npot)
	msgmerge -U --no-wrap --no-location --backup=none -q $@ $<
	@touch $@

$(I18Nmsgs): $(LOCALEDIR)/%/LC_MESSAGES/vdr-$(PLUGIN).mo: $(PODIR)/%.mo
	@mkdir -p $(dir $@)
	cp $< $@

.PHONY: i18n
i18n: $(I18Nmsgs) $(I18Npot)

### Targets:

libvdr-$(PLUGIN).so: $(JSONLIB) $(OBJS)
	$(CXX) $(CXXFLAGS) -shared $(OBJS) $(JSONLIB) $(LIBS) -o $@
ifndef ELVIS_DEBUG
	@$(STRIP) $@
endif
	@cp --remove-destination $@ $(LIBDIR)/$@.$(APIVERSION)

dist: clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean:
ifdef ELVIS_LIBJSONC
	$(MAKE) -C json clean
endif
	@-rm -f $(OBJS) $(DEPFILE) *.so *.tgz core* *~ $(PODIR)/*.mo $(PODIR)/*.pot
