#!/usr/bin/make -f
# Makefile for tetr4-switch.lv2 #
# Based on https://github.com/moddevices/mod-lv2-examples/blob/76653e0/examples/eg-amp.lv2/Makefile
# --------------------------------- #

include Makefile.mk

TETR4_SWITCH = tetr4-switch
GOSSIPER = gossiper

HEADERS=$(shell find -type f -name '*.h')
SOURCES=$(shell find -type f -name '*.c')
OBJECTS=$(shell find -type f -name '*.o')

# --------------------------------------------------------------
# Installation path

PREFIX  ?= /usr/local
DESTDIR ?=

INSTALL_PATH = /usr/local/lib/lv2
#COMPLETE_INSTALL_PATH = $(DESTDIR)$(INSTALL_PATH)/$(TETR4_SWITCH).lv2

# --------------------------------------------------------------
# Default target is to build all plugins

all: build
#build: build-gossiper
#build: build-tetr4-switch
build: build-gossiper build-tetr4-switch

build-tetr4-switch:
	$(CC) src/utils/utils.c \
		  src/lv2/controller/lv2-controller.c src/lv2/controller/atom-extension.c src/lv2/controller/lv2-hmi-extension.c \
		  src/model/controller.c \
		  src/tetr4-switch.c \
		  $(BUILD_C_FLAGS) $(LINK_FLAGS) -lm $(SHARED) -o resources/$(TETR4_SWITCH).lv2/$(TETR4_SWITCH).so

build-gossiper:
	$(CC) src/utils/utils.c \
		  src/lv2/gossiper/lv2-gossiper.c src/lv2/gossiper/lv2-hmi-gossiper.c \
		  src/model/gossiper.c \
		  src/gossiper.c \
		  $(BUILD_C_FLAGS) $(LINK_FLAGS) -lm $(SHARED) -o resources/$(GOSSIPER).lv2/$(GOSSIPER).so

# --------------------------------------------------------------

clean:
	rm -f $(TETR4_SWITCH).lv2/$(TETR4_SWITCH)$(LIB_EXT)
	rm -f $(GOSSIPER).lv2/$(GOSSIPER)$(LIB_EXT)

# --------------------------------------------------------------

install: build
	install -d $(DESTDIR)$(PREFIX)/lib/lv2/$(TETR4_SWITCH).lv2

	install -m 644 resources/$(TETR4_SWITCH).lv2/*.so  $(DESTDIR)$(PREFIX)/lib/lv2/$(TETR4_SWITCH).lv2/
	install -m 644 resources/$(TETR4_SWITCH).lv2/*.ttl $(DESTDIR)$(PREFIX)/lib/lv2/$(TETR4_SWITCH).lv2/

	cp -r resources/$(TETR4_SWITCH).lv2/modgui $(DESTDIR)$(PREFIX)/lib/lv2/$(TETR4_SWITCH).lv2/


	install -d $(DESTDIR)$(PREFIX)/lib/lv2/$(GOSSIPER).lv2

	install -m 644 resources/$(GOSSIPER).lv2/*.so  $(DESTDIR)$(PREFIX)/lib/lv2/$(GOSSIPER).lv2/
	install -m 644 resources/$(GOSSIPER).lv2/*.ttl $(DESTDIR)$(PREFIX)/lib/lv2/$(GOSSIPER).lv2/

	cp -r resources/$(GOSSIPER).lv2/modgui $(DESTDIR)$(PREFIX)/lib/lv2/$(GOSSIPER).lv2/

install-user: build
	install -d ~/.lv2/
	cp -r resources/$(TETR4_SWITCH).lv2/ ~/.lv2/$(TETR4_SWITCH).lv2
	cp -r resources/$(GOSSIPER).lv2/ ~/.lv2/$(GOSSIPER).lv2

# --------------------------------------------------------------

uninstall-user:
	rm -r ~/.lv2/$(TETR4_SWITCH).lv2
	rm -r ~/.lv2/$(GOSSIPER).lv2

# --------------------------------------------------------------
