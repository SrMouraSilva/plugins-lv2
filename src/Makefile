#!/usr/bin/make -f
# Makefile for plugin-tetr4-switch.lv2 #
# --------------------------------- #

include Makefile.mk

NAME = plugin-tetr4-switch

HEADERS=$(shell find -type f -name '*.h')
SOURCES=$(shell find -type f -name '*.c')
OBJECTS=$(shell find -type f -name '*.o')

# --------------------------------------------------------------
# Installation path

INSTALL_PATH = /usr/local/lib/lv2
COMPLETE_INSTALL_PATH = $(DESTDIR)$(INSTALL_PATH)/$(NAME).lv2

# --------------------------------------------------------------
# Default target is to build all plugins

all: build
build: $(SOURCES:.c=.o) $(NAME)-build

# --------------------------------------------------------------
# Build rules

%.o: %.c $(HEADERS)
	$(CC) -c $< $(BUILD_C_FLAGS) $(LINK_FLAGS) -lm $(SHARED) -o $@

$(NAME)-build: $(NAME).lv2/$(NAME)$(LIB_EXT)

$(NAME).lv2/$(NAME)$(LIB_EXT): $(NAME).c
	$(CC) -c $< $(BUILD_C_FLAGS) $(LINK_FLAGS) -lm $(SHARED) -o $@ 
#$(CC) $^ $(BUILD_C_FLAGS) $(LINK_FLAGS) -lm $(SHARED) -o $@

# --------------------------------------------------------------

clean:
	rm -f $(NAME).lv2/$(NAME)$(LIB_EXT)

# --------------------------------------------------------------

install: build
	install -d $(DESTDIR)$(PREFIX)/lib/lv2/$(NAME).lv2

	install -m 644 $(NAME).lv2/*.so  $(DESTDIR)$(PREFIX)/lib/lv2/$(NAME).lv2/
	install -m 644 $(NAME).lv2/*.ttl $(DESTDIR)$(PREFIX)/lib/lv2/$(NAME).lv2/

# --------------------------------------------------------------

