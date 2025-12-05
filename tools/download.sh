#!/bin/bash
# Script: download.sh
# Mục đích: Tải Kernel Source gốc và Raspberry Pi OS Image.

# Thoát ngay lập tức nếu bất kỳ lệnh nào thất bại
set -e

# --- ĐỊNH NGHĨA ĐƯỜNG DẪN CỤC BỘ ---
PROJECT_ROOT=$(dirname "$(dirname "$0")")
KERNEL_DIR="$PROJECT_ROOT/kernel/linux"
IMAGE_NAME="raspios_arm64.img"
IMAGE_PATH="$PROJECT_ROOT/images/$IMAGE_NAME"

echo "=== 📥 BẮT ĐẦU TẢI CÁC TÀI NGUYÊN LỚN ==="

# 1. CLONE MÃ NGUỒN KERNEL GỐC
if [ ! -d "$KERNEL_DIR" ]; then
    echo "1. Tải mã nguồn Kernel rpi-6.12.y..."
    cd "$PROJECT_ROOT/kernel"
    # Clone kernel source với branch và repo cố định
    git clone --depth=1 --branch rpi-6.12.y https://github.com/raspberrypi/linux.git linux
    echo "✅ Kernel Source đã được tải về."
else
    echo "✅ Kernel Source đã tồn tại, bỏ qua bước tải về."
fi

# 2. TẢI VÀ GIẢI NÉN IMAGE MẪU
if [ ! -f "$IMAGE_PATH" ]; then
    echo "2. Tải Raspberry Pi OS Image mẫu..."
    cd ~/pios/images
    # Tải file .xz từ URL cố định
    wget -nc https://downloads.raspberrypi.org/raspios_lite_arm64_latest -O "${IMAGE_NAME}.xz"
    echo "Giải nén Image..."
    # Giải nén
    unxz "${IMAGE_NAME}.xz"
    echo "✅ Image đã sẵn sàng."
else
    echo "✅ Image đã tồn tại, bỏ qua bước tải về."
fi

echo "=== ✅ TẢI TÀI NGUYÊN HOÀN TẤT ==="
