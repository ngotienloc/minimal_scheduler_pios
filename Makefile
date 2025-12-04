# Makefile
BPF_CLANG ?= clang

SRC_DIR   := bpf
OBJ_DIR   := build

SRC       := $(SRC_DIR)/mlfq.bpf.c
TARGET    := $(OBJ_DIR)/mlfq.bpf.o

INCLUDES  := -I. -Iinclude

CFLAGS = -O2 -g -target bpf -Wall -Werror $(INCLUDES)

.PHONY: all clean prepare

all: prepare $(TARGET)

prepare:
	mkdir -p $(OBJ_DIR)

$(TARGET): $(SRC)
	$(BPF_CLANG) $(CFLAGS) -c $(SRC) -o $(TARGET)

clean:
	rm -rf $(OBJ_DIR)
