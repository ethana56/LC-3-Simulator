.POSIX:
CC=clang
SRC_DIR=src
OBJ_DIR=obj
BIN_DIR=bin
DEVICES_DIR=$(BIN_DIR)/devices
PLUGINS=$(SRC_DIR)/plugins

EXE=$(BIN_DIR)/simulator
SRC=$(wildcard $(SRC_DIR)/*.c)
PLUGIN_SRC=$(wildcard $(PLUGINS)/*.c)
DEVICES=$(patsubst $(PLUGINS)/%.c, $(OBJ_DIR)/%.dylib, $(PLUGIN_SRC))
OBJ=$(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

CPPFLAGS=-MMD -MP
CFLAGS=-Wall -g -fsanitize=undefined -fsanitize=address
LDFLAGS=-ldl -g
DYLIBFLAGS=-dynamiclib -I ./src

.PHONY: all clean movedep

all: $(EXE) $(DEVICES)

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CC) $(OBJ) -o $@ $(CFLAGS) $(LDFLAGS)

$(OBJ_DIR)/%.dylib: $(PLUGINS)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DYLIBFLAGS) $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $@

$(OBJ_DIR):
	mkdir -p $@	

clean:
	@$(RM) -rv $(BIN_DIR) $(OBJ_DIR)

-include $(OBJ:.o=.d)
-include $(DEVICES:.dylib=.d)
