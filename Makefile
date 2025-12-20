OUTPUT := .output
CLANG ?= clang
BPFTOOL ?= bpftool

TARGET := mlfq_runner
BPF_SRC := bpf/mlfq.bpf.c
BPF_OBJ := $(OUTPUT)/mlfq.bpf.o
SKEL_H := $(OUTPUT)/mlfq.skel.h
VMLINUX_H := $(OUTPUT)/vmlinux.h

# Kiến trúc CPU (Pi 4 = arm64)
ARCH := $(shell uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/')

# Include Paths
# Sử dụng wildcard hoặc uname -r để đường dẫn header linh hoạt hơn
KERNEL_HEADERS := /usr/src/linux-headers-$(shell uname -r)/include
INCLUDES := -I$(OUTPUT) -I. -Ibpf -I/usr/include -I/usr/src/linux-headers-6.12.47+rpt-common-rpi/include

# Flags
BPF_CFLAGS := -g -O2 -target bpf -mcpu=v3 -D__TARGET_ARCH_$(ARCH) $(INCLUDES)
USER_CFLAGS := -g -O2 -Wall -I$(OUTPUT) -I. -I/usr/include

.PHONY: all clean

all: $(TARGET)

# 1. Quy tắc tạo thư mục output (Chỉ định nghĩa 1 lần duy nhất)
$(OUTPUT):
	mkdir -p $(OUTPUT)

# 2. Tạo vmlinux.h 
# Sử dụng '| $(OUTPUT)' (order-only prerequisite) để đảm bảo thư mục tồn tại
$(VMLINUX_H): | $(OUTPUT)
	@echo " GEN vmlinux.h"
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

# 3. Biên dịch BPF code (.c -> .o)
$(BPF_OBJ): $(BPF_SRC) $(VMLINUX_H) | $(OUTPUT)
	@echo " CLANG BPF-OBJ"
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

# 4. Tạo Skeleton header từ object file
$(SKEL_H): $(BPF_OBJ) | $(OUTPUT)
	@echo " GEN-SKEL"
	$(BPFTOOL) gen skeleton $< > $@

# 5. Biên dịch Loader (main.c)
$(TARGET): main.c $(SKEL_H)
	@echo " CC $(TARGET)"
	$(CLANG) $(USER_CFLAGS) $< -o $@ -lbpf -lelf -lz -lrt -pthread -z noexecstack

clean:
	rm -rf $(OUTPUT) $(TARGET)
