# Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
# Clean source distribution build; tests and generated artifacts are excluded.
CC ?= gcc
AR ?= ar
PKG_CONFIG ?= pkg-config
SOURCE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/Source)
INCLUDE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/Include)
BUILD_DIR ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/build)
BIN_DIR ?= $(BUILD_DIR)/bin
CFLAGS ?= -Wall -Wextra -Werror -O2 -std=c11
CPPFLAGS += -I$(INCLUDE_DIR) -I$(SOURCE_DIR) $(shell $(PKG_CONFIG) --cflags freetype2)
LDLIBS += $(shell $(PKG_CONFIG) --libs freetype2) -lutil
CORE_NAMES := weact_display text_freetype weact_graphics weact_fastlz weact_surface weact_protocol weact_extended weact_init weact_memory_transport weact_posix_transport
CORE_SRC := $(addprefix $(SOURCE_DIR)/,$(addsuffix .c,$(CORE_NAMES)))
CORE_OBJ := $(addprefix $(BUILD_DIR)/,$(addsuffix .pic.o,$(CORE_NAMES)))
CORE_STATIC_OBJ := $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(CORE_NAMES)))
SHARED := $(BUILD_DIR)/libweact.so.1.0.0
STATIC := $(BUILD_DIR)/libweact.a
CLI_SRC := $(SOURCE_DIR)/tools/weact_cli.c $(SOURCE_DIR)/tools/weact_cli_parse.c $(SOURCE_DIR)/tools/weact_cli_render.c $(SOURCE_DIR)/tools/weact_cli_transport.c $(SOURCE_DIR)/tools/weact_cli_codes.c $(SOURCE_DIR)/weact_codes.c $(SOURCE_DIR)/codes/render.c
CLI_LIBS := $(shell $(PKG_CONFIG) --libs freetype2) -l:libgdk_pixbuf-2.0.so.0 -l:libgobject-2.0.so.0 -lm -l:libqrencode.so.4
.PHONY: all clean check-deps
all: $(SHARED) $(STATIC) $(BIN_DIR)/weact-cli $(BIN_DIR)/weact-list $(BIN_DIR)/weactterm $(BIN_DIR)/weact-fs35-features
check-deps:
	$(PKG_CONFIG) --exists freetype2
	$(PKG_CONFIG) --exact-version=0.3.3 vterm
$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@
$(BUILD_DIR)/%.pic.o: $(SOURCE_DIR)/%.c | $(BUILD_DIR) check-deps
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -MMD -MP -c $< -o $@
$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c | $(BUILD_DIR) check-deps
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
$(SHARED): $(CORE_OBJ) $(SOURCE_DIR)/weact.map
	$(CC) -shared $(LDFLAGS) -Wl,-soname,libweact.so.1 -Wl,--version-script=$(SOURCE_DIR)/weact.map -o $@ $(CORE_OBJ) $(shell $(PKG_CONFIG) --libs freetype2)
	ln -sf libweact.so.1.0.0 $(BUILD_DIR)/libweact.so.1
	ln -sf libweact.so.1 $(BUILD_DIR)/libweact.so
$(STATIC): $(CORE_STATIC_OBJ)
	$(AR) rcs $@ $^
$(BIN_DIR)/weact-cli: $(CLI_SRC) $(SHARED) | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CLI_SRC) -L$(BUILD_DIR) -lweact -Wl,-rpath-link,$(BUILD_DIR) $(LDFLAGS) $(CLI_LIBS) -o $@
$(BIN_DIR)/weact-list: $(SOURCE_DIR)/tools/weact_list.c | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LDFLAGS) -o $@
$(BIN_DIR)/weactterm: $(SOURCE_DIR)/tools/weactterm.c $(SOURCE_DIR)/tools/weactterm_screen.c $(SOURCE_DIR)/tools/weactterm_input.c $(SOURCE_DIR)/tools/weactterm_pty.c $(SOURCE_DIR)/tools/weactterm_presenter.c $(STATIC) | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I$(SOURCE_DIR)/tools $^ $(LDFLAGS) -lvterm $(shell $(PKG_CONFIG) --libs freetype2) -lutil -pthread -o $@
$(BIN_DIR)/weact-fs35-features: $(SOURCE_DIR)/tools/weact_fs35_features.c $(STATIC) | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I$(SOURCE_DIR)/tools $^ $(LDFLAGS) -o $@
clean:
	rm -rf $(BUILD_DIR)
-include $(CORE_OBJ:.pic.o=.pic.d) $(CORE_STATIC_OBJ:.o=.d)
