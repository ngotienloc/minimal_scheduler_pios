#!/bin/bash

# 1. Kiểm tra xem bpftool đã cài chưa
if ! command -v bpftool &> /dev/null; then
    echo "LỖI: Không tìm thấy lệnh 'bpftool'."
    echo "Hãy cài đặt package: linux-tools-common hoặc linux-tools-generic"
    exit 1
fi

# 2. Kiểm tra xem kernel có hỗ trợ BTF không
if [ ! -f /sys/kernel/btf/vmlinux ]; then
    echo "LỖI: Không tìm thấy /sys/kernel/btf/vmlinux."
    echo "Kernel hiện tại không hỗ trợ BTF hoặc chưa bật CONFIG_DEBUG_INFO_BTF."
    exit 1
fi

# 3. Thực hiện tạo file
echo "Đang tạo vmlinux.h từ kernel đang chạy..."
bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

# 4. Kiểm tra kết quả
if [ $? -eq 0 ]; then
    echo "✅ vmlinux.h generated successfully!"
else
    echo "❌ Lỗi khi tạo vmlinux.h"
    exit 1
fi