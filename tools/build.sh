#!/bin/bash
set -euo pipefail
IFS=$'\n\t'

# =========================================================
# PIOS MLFQ: BUILD & DEPLOY KERNEL VÀO IMAGE MẪU (RPi4)
# Phiên bản đã fix build clean + auto_group.o
# =========================================================

ARCH="arm64"
CROSS_COMPILE="aarch64-linux-gnu-"
NUM_JOBS="$(nproc)"

KERNEL_DIR="$HOME/pios/kernel/linux"
BUILD_DIR="$HOME/pios/build_artifacts"
DEVICE_TREE_DIR="$HOME/pios/device-tree"

IMG_NAME="raspios_arm64.img"
IMG_PATH="$HOME/pios/images/$IMG_NAME"

MOUNT_BOOT="/mnt/boot"
MOUNT_ROOT="/mnt/root"

# --- Cleanup ---
cleanup() {
    echo "--- Cleanup: unmount & detach loop device ---"
    sudo umount "$MOUNT_BOOT" 2>/dev/null || true
    sudo umount "$MOUNT_ROOT" 2>/dev/null || true
    if [ -n "${LOOP_DEV:-}" ]; then
        sudo losetup -d "$LOOP_DEV" 2>/dev/null || true
    fi
    echo "✔ Cleanup xong."
}
trap cleanup EXIT INT TERM

# --- 0. Patch MLFQ: loại bỏ auto_group.o nếu cần ---
SCHED_MAKEFILE="$KERNEL_DIR/kernel/sched/Makefile"
if grep -q "auto_group.o" "$SCHED_MAKEFILE"; then
    echo "--- Loại bỏ auto_group.o khỏi kernel/sched/Makefile ---"
    sed -i '/auto_group.o/d' "$SCHED_MAKEFILE"
fi

# --- 1. Build kernel ---
cd "$KERNEL_DIR"
echo "1. Clean build kernel"
make ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" mrproper
echo "2. Sử dụng bcm2711_defconfig cho RPi4"
make ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" bcm2711_defconfig

# Copy .config sang build_artifacts
mkdir -p "$BUILD_DIR"
cp .config "$BUILD_DIR/"

echo "3. Biên dịch Image, DTB, Modules"
make -j"$NUM_JOBS" ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" Image dtbs modules

# Copy Image và DTB sang thư mục build_artifacts / device-tree
cp "arch/$ARCH/boot/Image" "$BUILD_DIR/"
mkdir -p "$DEVICE_TREE_DIR"
cp arch/$ARCH/boot/dts/broadcom/*.dtb "$DEVICE_TREE_DIR/"

# Cài đặt modules vào build_artifacts
make ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE" INSTALL_MOD_PATH="$BUILD_DIR" modules_install

echo "✅ Build kernel xong."

# --- 2. Setup Image Rootfs ---
if [ ! -f "$IMG_PATH" ]; then
    echo "❌ Không tìm thấy file image: $IMG_PATH"
    exit 1
fi

LOOP_DEV=$(sudo losetup --show -fP "$IMG_PATH")
echo "Device loop: $LOOP_DEV"

sudo mkdir -p "$MOUNT_BOOT" "$MOUNT_ROOT"
sudo mount "${LOOP_DEV}p1" "$MOUNT_BOOT"
sudo mount "${LOOP_DEV}p2" "$MOUNT_ROOT"

echo "🎉 Mount xong."

# --- 3. Copy Kernel / DTB / Modules ---
echo "Copy kernel mới vào boot partition"
sudo cp "$BUILD_DIR/Image" "$MOUNT_BOOT/kernel8.img"

echo "Copy Device Tree Pi4"
sudo cp "$DEVICE_TREE_DIR/bcm2711-rpi-4-b.dtb" "$MOUNT_BOOT/"

echo "Copy Modules vào rootfs"
sudo mkdir -p "$MOUNT_ROOT/lib/modules"
sudo rm -rf "$MOUNT_ROOT/lib/modules/"*

if [ -d "$BUILD_DIR/lib/modules" ]; then
    sudo cp -r "$BUILD_DIR/lib/modules/"* "$MOUNT_ROOT/lib/modules/"
fi

echo "🎉 Triển khai xong."
