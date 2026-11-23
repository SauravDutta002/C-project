# RailNet PMS - Railway Platform Management System
#   make          build   (mingw32-make on Windows)
#   make run      build and run
#   make clean    remove build output

ifeq ($(origin CC),default)
    CC := gcc
endif
CFLAGS  := -std=gnu99 -Wall -Wextra -O2 -Iinclude -MMD -MP
LDFLAGS :=

SRC := $(wildcard src/*.c src/*/*.c)
OBJ := $(patsubst src/%.c,build/obj/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

ifeq ($(OS),Windows_NT)
    TARGET   := build/railnet.exe
    MKDIR_P   = if not exist "$(subst /,\,$(1))" mkdir "$(subst /,\,$(1))"
    RM_BUILD := if exist build rmdir /s /q build
    RUN      := build\railnet.exe
else
    TARGET   := build/railnet
    MKDIR_P   = mkdir -p $(1)
    RM_BUILD := rm -rf build
    RUN      := ./build/railnet
endif

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)
	@echo Built $@

build/obj/%.o: src/%.c
	@$(call MKDIR_P,$(dir $@))
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	$(RUN)

clean:
	$(RM_BUILD)

-include $(DEP)
