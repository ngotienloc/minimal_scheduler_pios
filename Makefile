OUTPUT := .output
CLANG ?= clang
BPFTOOL ?= bpftool

TARGET := mlfq_runner
BPF_SRC := mlfq.bpf.c
BPF_OBJ := $(OUTPUT)/mlfq.bpf.o
SKEL_H := mlfq.skel.h

# Detect arch (Pi 4 = aarch64 → arm64)
ARCH := $(shell uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/')
INCLUDES := -I$(OUTPUT) -I.

CFLAGS := -g -O2 -Wall
BPF_CFLAGS := -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) $(INCLUDES)

all: $(TARGET)

# Create output dir
$(OUTPUT):
	mkdir -p $(OUTPUT)

# Generate vmlinux.h
$(OUTPUT)/vmlinux.h: | $(OUTPUT)
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

# Compile BPF to .o
$(BPF_OBJ): $(BPF_SRC) $(OUTPUT)/vmlinux.h
	$(CLANG) $(BPF_CFLAGS) -c $(BPF_SRC) -o $@

# Generate skeleton
$(SKEL_H): $(BPF_OBJ)
	$(BPFTOOL) gen skeleton $(BPF_OBJ) > $@

# Compile userspace loader
$(TARGET): main.c $(SKEL_H)
	$(CLANG) $(CFLAGS) main.c -o $@ -lbpf -lelf -lz -z noexecstack

clean:
	rm -rf $(OUTPUT) $(TARGET) $(SKEL_H)

.PHONY: all clean
