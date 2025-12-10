# --- CẤU HÌNH ---
OUTPUT := .output
CLANG ?= clang
BPFTOOL ?= bpftool

# Tên file loader chạy trên userspace
TARGET := mlfq_runner

# Tên file workload test (tự động biên dịch cpu_bound)
TEST_TARGET := cpu_bound

# ĐƯỜNG DẪN QUAN TRỌNG (Dựa theo ảnh của bạn)
BPF_SRC := bpf/mlfq.bpf.c
TEST_SRC := examples/cpu_bound.c

# File kết quả trung gian
BPF_OBJ := $(OUTPUT)/mlfq.bpf.o
SKEL_H := $(OUTPUT)/mlfq.skel.h

# Kiến trúc CPU (Pi 4 = arm64)
ARCH := $(shell uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/')

# Include Path:
# -I$(OUTPUT): để tìm vmlinux.h, mlfq.skel.h
# -Ibpf: để tìm scx_common.bpf.h (nếu bạn để nó trong thư mục bpf/)
INCLUDES := -I$(OUTPUT) -I. -Ibpf

CFLAGS := -g -O2 -Wall $(INCLUDES)
BPF_CFLAGS := -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) $(INCLUDES)

# --- RULES ---

all: $(TARGET) $(TEST_TARGET)

# 1. Tạo thư mục output
$(OUTPUT):
	mkdir -p $(OUTPUT)

# 2. Tạo vmlinux.h (Cần thiết cho BPF)
$(OUTPUT)/vmlinux.h: | $(OUTPUT)
	@echo "  GEN     vmlinux.h"
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

# 3. Biên dịch BPF code (.c -> .o)
$(BPF_OBJ): $(BPF_SRC) $(OUTPUT)/vmlinux.h
	@echo "  CLANG   BPF-OBJ"
	$(CLANG) $(BPF_CFLAGS) -c $(BPF_SRC) -o $@

# 4. Tạo Skeleton (.h) để main.c sử dụng
$(SKEL_H): $(BPF_OBJ)
	@echo "  GEN-SKEL"
	$(BPFTOOL) gen skeleton $(BPF_OBJ) > $@

# 5. Biên dịch Loader (main.c)
$(TARGET): main.c $(SKEL_H)
	@echo "  CC      $(TARGET)"
	$(CLANG) $(CFLAGS) main.c -o $@ -lbpf -lelf -lz -z noexecstack

# 6. Biên dịch Test Workload (cpu_bound.c)
$(TEST_TARGET): $(TEST_SRC)
	@echo "  CC      $(TEST_TARGET)"
	$(CLANG) -O2 $(TEST_SRC) -o $(TEST_TARGET)

# Dọn dẹp
clean:
	rm -rf $(OUTPUT) $(TARGET) $(TEST_TARGET)

.PHONY: all clean