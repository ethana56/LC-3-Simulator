.POSIX:
CC=cc
SRC_DIR=src
OBJ_DIR=obj
BIN_DIR=bin
DEVICES_DIR=devices

EXE=$(BIN_DIR)/simulator
SRC=$(wildcard $(SRC_DIR)/*.c)
OBJ=$(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

CPPFLAGS=-MMD -MP
CFLAGS=-Wall -Werror -ldl -lpthread -g

.PHONY: all clean

all: $(EXE)

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CC) $(OBJ) -o $@ $(CFLAGS) 

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $@/$(DEVICES_DIR)

$(OBJ_DIR):
	mkdir -p $@	

clean:
	@$(RM) -rv $(BIN_DIR) $(OBJ_DIR)

-include $(OBJ:.o=.d)
