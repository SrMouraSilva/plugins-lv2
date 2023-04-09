#!/usr/bin/make -f
# Makefile for tetr4-switch.lv2 #
# Based on https://github.com/moddevices/mod-lv2-examples/blob/76653e0/examples/eg-amp.lv2/Makefile
# --------------------------------- #

include Makefile.mk

NAME = tetr4-switch

HEADERS=$(shell find -type f -name '*.h')
SOURCES=$(shell find -type f -name '*.c')
OBJECTS=$(shell find -type f -name '*.o')

# --------------------------------------------------------------
# Installation path

PREFIX  ?= /usr/local
DESTDIR ?=

INSTALL_PATH = /usr/local/lib/lv2
COMPLETE_INSTALL_PATH = $(DESTDIR)$(INSTALL_PATH)/$(NAME).lv2

# --------------------------------------------------------------
# Default target is to build all plugins

all: build
build:
	$(CC) utils/utils.c extension/atom-extension.c extension/lv2-hmi-extension.c model/tetr4-switch.c tetr4-switch.c $(BUILD_C_FLAGS) $(LINK_FLAGS) -lm $(SHARED) -o $(NAME).lv2/$(NAME).so

# --------------------------------------------------------------

clean:
	rm -f $(NAME).lv2/$(NAME)$(LIB_EXT)

# --------------------------------------------------------------

install: build
	install -d $(DESTDIR)$(PREFIX)/lib/lv2/$(NAME).lv2

	install -m 644 $(NAME).lv2/*.so  $(DESTDIR)$(PREFIX)/lib/lv2/$(NAME).lv2/
	install -m 644 $(NAME).lv2/*.ttl $(DESTDIR)$(PREFIX)/lib/lv2/$(NAME).lv2/

	cp -r $(NAME).lv2/modgui $(DESTDIR)$(PREFIX)/lib/lv2/$(NAME).lv2/

install-user: build
	install -d ~/.lv2/
	cp -r $(NAME).lv2/ ~/.lv2/$(NAME).lv2

# --------------------------------------------------------------

uninstall-user:
	rm -r ~/.lv2/$(NAME).lv2

# --------------------------------------------------------------
