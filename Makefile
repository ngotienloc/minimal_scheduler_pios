# Compiler settings
BPF_CLANG ?= clang
BPF_LLVM  ?= llc

# Paths
SRC_DIR   := bpf
OBJ_DIR   := build

SRC       := $(SRC_DIR)/mlfq.bpf.c
TARGET    := $(OBJ_DIR)/mlfq.bpf.o

# Includes for vmlinux.h + scx_common
INCLUDES  := -I. -Iinclude

# Build flags recommended by Kernel team (stable)
CFLAGS = -O2 -g -target bpf \
         -Wall -Werror \
         -D__TARGET_ARCH_x86 \
         $(INCLUDES)

.PHONY: all clean prepare

all: prepare $(TARGET)

prepare:
	mkdir -p $(OBJ_DIR)

$(TARGET): $(SRC)
	$(BPF_CLANG) $(CFLAGS) -c $(SRC) -o $(TARGET)

clean:
	rm -rf $(OBJ_DIR)
