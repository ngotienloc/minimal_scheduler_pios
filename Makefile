# --- CẤU HÌNH ---
OUTPUT := .output
CLANG ?= clang
BPFTOOL ?= bpftool

# Tên file loader chạy trên userspace
TARGET := mlfq_runner

# Tên file workload test (Tạm tắt nếu chưa có file code)
# TEST_TARGET := cpu_bound

# ĐƯỜNG DẪN (Đã chỉnh lại cho dễ dùng)
# Giả sử bạn để file .bpf.c ngay thư mục hiện tại hoặc trong thư mục bpf/
# Nếu bạn để file ngay thư mục ngoài, hãy sửa dòng dưới thành: BPF_SRC := mlfq.bpf.c
BPF_SRC := bpf/mlfq.bpf.c

# File kết quả trung gian
BPF_OBJ := $(OUTPUT)/mlfq.bpf.o
SKEL_H := $(OUTPUT)/mlfq.skel.h

# Kiến trúc CPU (Pi 4 = arm64)
ARCH := $(shell uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/')

# Include Path:
INCLUDES := -I$(OUTPUT) -I. -Ibpf

CFLAGS := -g -O2 -Wall $(INCLUDES)

# !!! FIX QUAN TRỌNG: Thêm -mcpu=v3 !!!
BPF_CFLAGS := -g -O2 -target bpf -mcpu=v3 -D__TARGET_ARCH_$(ARCH) $(INCLUDES)

# --- RULES ---

all: $(TARGET)

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
# FIX: Thêm -lrt -pthread để tránh lỗi linker
$(TARGET): main.c $(SKEL_H)
	@echo "  CC      $(TARGET)"
	$(CLANG) $(CFLAGS) main.c -o $@ -lbpf -lelf -lz -lrt -pthread -z noexecstack

# Dọn dẹp
clean:
	rm -rf $(OUTPUT) $(TARGET)

.PHONY: all clean