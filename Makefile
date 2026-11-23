# RailNet PMS - Railway Platform Management System
#   make            build the desktop app (Windows) and the console app
#   make gui        desktop app only     -> build/RailNetPMS.exe
#   make console    console app only     -> build/railnet_console.exe
#   make run        build and start the desktop app
#   make clean

ifeq ($(origin CC),default)
    CC := gcc
endif
WINDRES ?= windres

CFLAGS  := -std=gnu99 -Wall -Wextra -O2 -Iinclude -MMD -MP

CORE_SRC    := $(wildcard src/core/*.c src/io/*.c src/util/*.c)
CONSOLE_SRC := src/main.c $(wildcard src/ui/*.c)
GUI_SRC     := $(wildcard src/gui/*.c)

obj = $(patsubst src/%.c,build/obj/%.o,$(1))
CORE_OBJ    := $(call obj,$(CORE_SRC))
CONSOLE_OBJ := $(call obj,$(CONSOLE_SRC))
GUI_OBJ     := $(call obj,$(GUI_SRC))
RES_OBJ     := build/obj/app_res.o
DEP         := $(CORE_OBJ:.o=.d) $(CONSOLE_OBJ:.o=.d) $(GUI_OBJ:.o=.d)

ifeq ($(OS),Windows_NT)
    EXE      := .exe
    MKDIR_P   = if not exist "$(subst /,\,$(1))" mkdir "$(subst /,\,$(1))"
    RM_BUILD := if exist build rmdir /s /q build
    DEFAULT  := gui console
else
    EXE      :=
    MKDIR_P   = mkdir -p $(1)
    RM_BUILD := rm -rf build
    DEFAULT  := console
endif

GUI_BIN     := build/RailNetPMS$(EXE)
CONSOLE_BIN := build/railnet_console$(EXE)

.PHONY: all gui console run clean

all: $(DEFAULT)
gui: $(GUI_BIN)
console: $(CONSOLE_BIN)

$(GUI_BIN): $(CORE_OBJ) $(GUI_OBJ) $(RES_OBJ)
	$(CC) $^ -o $@ -mwindows -lcomctl32 -lgdi32 -lshell32
	@echo Built $@

$(CONSOLE_BIN): $(CORE_OBJ) $(CONSOLE_OBJ)
	$(CC) $^ -o $@
	@echo Built $@

build/obj/%.o: src/%.c
	@$(call MKDIR_P,$(dir $@))
	$(CC) $(CFLAGS) -c $< -o $@

$(RES_OBJ): resources/app.rc resources/app.manifest resources/app.ico include/gui/resource.h
	@$(call MKDIR_P,build/obj)
	$(WINDRES) -Iinclude -Iresources $< -O coff -o $@

run: gui
	$(GUI_BIN)

clean:
	$(RM_BUILD)

-include $(DEP)
