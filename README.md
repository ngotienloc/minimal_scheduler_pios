# Custom Raspberry Pi Kernel Build (SCX & eBPF Ready)

Dự án này cung cấp hướng dẫn chi tiết để build **Linux Kernel 6.12.y** tùy chỉnh cho Raspberry Pi 4.
Kernel này được tối ưu hóa "sạch" và cấu hình đặc biệt để hỗ trợ:

* **sched_ext (SCX):** Cơ chế lập lịch mở rộng (Extensible Scheduling Class) cho phép chạy scheduler tùy chỉnh từ userspace.
* **eBPF (Extended BPF):** Hỗ trợ đầy đủ CO-RE (Compile Once – Run Everywhere) và BTF (BPF Type Format).

## 📂 1. Cấu Trúc Thư Mục

Để đảm bảo các lệnh trong hướng dẫn hoạt động chính xác, chúng ta sử dụng cấu trúc thư mục sau:

```text
pios/
├── kernel/
│   └── linux/            # Source code kernel (Branch rpi-6.12.y)
│
├── build_artifacts/      # Nơi chứa kết quả sau khi build
│   ├── .config
│   ├── Image
│   ├── System.map
│   ├── vmlinux
│   └── modules/
│
├── device-tree/          # Device Tree Blobs
│   ├── *.dtb
│   └── overlays/         # DTB Overlays (BẮT BUỘC cho Pi 4)
│       └── *.dtbo
│
└── images/
    └── raspios_arm64.img # Image gốc (sẽ được thay ruột kernel)
🛠️ 2. Chuẩn Bị Môi Trường (Host Machine)
Thực hiện trên máy tính dùng để build (Ubuntu/Debian/WSL).

2.1. Cài đặt các gói phụ thuộc
Bash

sudo apt update
sudo apt install -y build-essential bc bison flex libssl-dev libncurses-dev \
libelf-dev libelf1 dwarves device-tree-compiler git rsync python3 python3-pip \
gcc-aarch64-linux-gnu
Lưu ý: Gói dwarves là bắt buộc để có công cụ pahole giúp tạo BTF information cho eBPF.

2.2. Tạo thư mục và tải Source Code
Bash

# Tạo cấu trúc thư mục
mkdir -p ~/pios/{kernel,build_artifacts,device-tree/overlays,images}

# Clone Kernel (Branch rpi-6.12.y)
cd ~/pios/kernel/
git clone --depth=1 --branch rpi-6.12.y [https://github.com/raspberrypi/linux.git](https://github.com/raspberrypi/linux.git) linux

# Tải OS Image (Raspberry Pi OS Lite 64-bit)
cd ~/pios/images/
wget [https://downloads.raspberrypi.com/raspios_lite_arm64_latest](https://downloads.raspberrypi.com/raspios_lite_arm64_latest) -O raspios_arm64.img.xz
unxz raspios_arm64.img.xz
⚙️ 3. Cấu Hình Kernel (Configuration)
Thực hiện đúng trình tự: Defconfig trước, sau đó mới Menuconfig.

3.1. Load cấu hình chuẩn RPi 4
Bash

cd ~/pios/kernel/linux/
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- bcm2711_defconfig
3.2. Tùy chỉnh Menuconfig
Bash

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- menuconfig
⚠️ QUAN TRỌNG: Thực hiện các bước sau trong giao diện Menuconfig:

Đặt tên phiên bản (Custom Tag):

Đi tới: General setup → Local version - append to kernel release.

Nhập: -scx-custom-mlfq (Sau này uname -r sẽ hiện: 6.12.y-scx-custom-mlfq).

Bật Debug Information (BẮT BUỘC ĐỂ HIỆN MENU BTF):

Đi tới: Kernel hacking → Compile-time checks and compiler options → Debug information.

Nếu đang là "Disable debug information", hãy bấm vào và chọn DWARF Version 4.

Bật BTF (Cho eBPF CO-RE):

Ngay bên dưới mục Debug information vừa chỉnh, dòng này sẽ hiện ra.

Bật [*] Generate BTF type information (Nhấn phím Y).

Bật BPF Subsystem:

Đi tới: General setup → BPF subsystem.

Đảm bảo bật [*] Enable BPF Just In Time compiler.

Đảm bảo bật [*] BPF syscall.

Bật Sched_ext (SCX):

Đi tới: General setup → Scheduler features.

Bật [*] Extensible Scheduling Class.

Lưu ý: Sau khi cấu hình xong: Chọn <Save>, bấm OK, rồi chọn <Exit> liên tục để thoát.

🏗️ 4. Build Kernel
Quá trình này mất khoảng 20-40 phút tùy vào sức mạnh CPU của máy host.

Bash

# 1. Build Image, Modules, và Device Trees
make -j$(nproc) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image modules dtbs

# 2. Copy kết quả ra thư mục quản lý (Artifacts)
cp .config ../../build_artifacts/
cp arch/arm64/boot/Image ../../build_artifacts/
cp arch/arm64/boot/dts/broadcom/*.dtb ../../device-tree/
# Copy Overlays (Quan trọng: Nếu thiếu Pi 4 sẽ không boot được)
cp arch/arm64/boot/dts/broadcom/overlays/*.dtbo ../../device-tree/overlays/
cp arch/arm64/boot/dts/broadcom/overlays/README ../../device-tree/overlays/

# 3. Install Modules vào thư mục tạm
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- INSTALL_MOD_PATH=../../build_artifacts modules_install
📦 5. Đóng Gói Vào OS Image
Bước này sẽ mount file .img ra như một ổ đĩa và thay thế kernel cũ bằng kernel vừa build.

Bash

cd ~/pios/images
IMG=raspios_arm64.img

# 1. Mount Image vào Loop Device
LOOP_DEV=$(sudo losetup --show -fP $IMG)
echo "Image mounted at: $LOOP_DEV"

# 2. Mount các phân vùng
sudo mkdir -p /mnt/boot /mnt/root
sudo mount ${LOOP_DEV}p1 /mnt/boot  # Phân vùng Boot (FAT32)
sudo mount ${LOOP_DEV}p2 /mnt/root  # Phân vùng Rootfs (EXT4)

# 3. Copy Kernel (Ghi đè kernel8.img)
sudo cp ~/pios/build_artifacts/Image /mnt/boot/kernel8.img

# 4. Copy Device Tree & Overlays (BẮT BUỘC ĐỦ 2 MỤC)
sudo cp ~/pios/device-tree/*.dtb /mnt/boot/
sudo cp ~/pios/device-tree/overlays/*.dtbo /mnt/boot/overlays/
sudo cp ~/pios/device-tree/overlays/README /mnt/boot/overlays/

# 5. Copy Modules
sudo cp -r ~/pios/build_artifacts/lib/modules/* /mnt/root/lib/modules/

# 6. Cấu hình boot (Ép chạy chế độ 64-bit)
# Lệnh này thêm 'arm_64bit=1' vào cuối file nếu chưa có
grep -qxF 'arm_64bit=1' /mnt/boot/config.txt || echo 'arm_64bit=1' | sudo tee -a /mnt/boot/config.txt

# 7. Dọn dẹp & Unmount
sudo umount /mnt/boot
sudo umount /mnt/root
sudo losetup -d $LOOP_DEV
echo "✅ Hoàn tất! File raspios_arm64.img đã sẵn sàng để flash."
✅ 6. Kiểm Tra & Chạy Thử
Flash file raspios_arm64.img vào thẻ nhớ (dùng Raspberry Pi Imager hoặc Etcher).

Cắm thẻ vào Raspberry Pi 4 và khởi động.

SSH vào Pi và chạy các lệnh kiểm tra sau:

Kiểm tra phiên bản kernel:

Bash

uname -r
# Output kỳ vọng: 6.12.y-scx-custom-mlfq
Kiểm tra hỗ trợ SCX:

Bash

ls /sys/kernel/sched_ext/
# Nếu thư mục này tồn tại -> SCX đã hoạt động.
Kiểm tra hỗ trợ BTF (cho eBPF):

Bash

ls -lh /sys/kernel/btf/vmlinux
# Nếu thấy file vmlinux (kích thước ~4-6MB) -> BTF OK.
