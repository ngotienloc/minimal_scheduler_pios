# Build Kernel Tuỳ Chỉnh cho Raspberry Pi (Bản Tối Giản)

Dự án này cung cấp **cấu trúc tối giản và sạch** để build **kernel Raspberry Pi 6.12.y** và đóng gói thành một file `.img` có thể boot trên Raspberry Pi 4.

Dự án **không** bao gồm bất kỳ cơ chế lập lịch tùy chỉnh nào (MLFQ, SCX, eBPF). Mục tiêu chỉ gồm:

* Build kernel Raspberry Pi 4
* Xuất Image, DTB và modules
* Chèn kernel mới vào file Raspberry Pi OS `.img`
* Boot Raspberry Pi với kernel tự build

---

# 📁 Cấu Trúc Thư Mục

```
pios/
├── kernel/
│   ├── linux/            # Source kernel
│   └── build.sh          # Script build kernel (tùy chọn)
│
├── build_artifacts/      # Output build kernel
│   ├── .config
│   ├── Image
│   ├── System.map
│   ├── vmlinux
│   └── modules/
│
├── device-tree/          # DTB / DTBO sau khi build
│   └── overlays/
│       └── *.dtbo
│
├── sdcard/               # Điểm mount khi chỉnh sửa OS image
│   ├── boot/
│   └── rootfs/
│
└── tools/                # Script hỗ trợ (mount/copy)
    ├── mount.sh
    ├── umount.sh
    └── copy-kernel.sh
```

---

# ⚙️ Chuẩn Bị Môi Trường

Cài các gói cần thiết:

```
sudo apt update
sudo apt install -y build-essential bc bison flex libssl-dev libncurses-dev libelf-dev libelf1 dwarves device-tree-compiler git rsync python3 python3-pip gcc-aarch64-linux-gnus-dev \
libelf-dev libelf1 dwarves device-tree-compiler \
git rsync python3 python3-pip
```

---

# 📂 Tạo Cây Thư Mục

```
mkdir -p pios/{kernel/linux,build_artifacts,device-tree/overlays,sdcard/{boot,rootfs},tools}
```

---

# 🧩 Clone Kernel

```
cd ~/pios/kernel/
git clone --depth=1 --branch rpi-6.12.y https://github.com/raspberrypi/linux.git linux
```

---

# 🔧 Configure Kernel

## 🔧 Bật hỗ trợ **sched_ext (SCX)** trong menuconfig

> *Phần này chỉ cần nếu bạn muốn kernel tự build hỗ trợ chạy các scheduler dựa trên SCX/eBPF.*

Chạy menuconfig:

```
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- menuconfig
```

Trong menu, bật các mục sau:
Cần bật Kernel hacking trước: 
Ấn / vòa tìm kiếm tìm: CONFIG_DEBUG_INFO => Ấn 1 rồi tắt Reduce debugging information => bật [*] Generate BTF type information   
Sau đó quay lại General setup bật      [ ] Extensible Scheduling Class (NEW)   
### 1️⃣ Bật hệ thống BPF

* **General setup → BPF subsystem**

  * ✔ `CONFIG_BPF=y`
  * ✔ `CONFIG_BPF_SYSCALL=y`
  * ✔ `CONFIG_BPF_JIT=y`

### 2️⃣ Bật sched_ext

* **General setup → Scheduler features**

  * ✔ `CONFIG_SCHED_CLASS_EXT=y`

### 3️⃣ Bật Debug Info BTF (bắt buộc cho eBPF struct_ops)

* **Kernel hacking → Compile-time checks and instrumentation**

  * ✔ `CONFIG_DEBUG_INFO_BTF=y`

### 4️⃣ (Khuyến nghị) Bật thêm debug để hỗ trợ eBPF

* **Kernel hacking**

  * ✔ `CONFIG_DEBUG_INFO=y`
  * ✔ `CONFIG_DEBUG_INFO_DWARF4=y`

Sau khi bật xong, lưu cấu hình (Save) và thoát. Tiếp tục build kernel như bình thường.

```
cd ~/pios/kernel/linux/
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- bcm2711_defconfig
cp .config ../../build_artifacts/
```

---

# 🏗️ Build Kernel, DTBs, Modules

Build tất cả:

```
make -j$(nproc) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image dtbs modules
```

Copy output:

```
cp arch/arm64/boot/Image ../../build_artifacts/
cp arch/arm64/boot/dts/broadcom/*.dtb ../../device-tree/
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- INSTALL_MOD_PATH=../../build_artifacts/ modules_install
```

---

# 📦 Chuẩn Bị OS Image Raspberry Pi

```
cd ~/pios
mkdir images
cd images

wget https://downloads.raspberrypi.com/raspios_lite_arm64_latest -O raspios_arm64.img.xz
unxz raspios_arm64.img.xz
```

---

# 🔗 Mount OS Image

```
cd ~/pios/images
IMG=raspios_arm64.img
LOOP_DEV=$(sudo losetup --show -fP $IMG)
echo "Device loop: $LOOP_DEV"

sudo mkdir -p /mnt/boot /mnt/root
sudo mount ${LOOP_DEV}p1 /mnt/boot
sudo mount ${LOOP_DEV}p2 /mnt/root
```

---

# 📁 Copy Kernel + Modules vào Image

Thay kernel:

```
sudo cp ~/pios/build_artifacts/Image /mnt/boot/kernel8.img
```

Copy Device Tree:

```
sudo cp ~/pios/device-tree/*.dtb /mnt/boot/
```

Copy Modules:

```
sudo cp -r ~/pios/build_artifacts/lib/modules/* /mnt/root/lib/modules/
```

---

# 🧹 Unmount + Cleanup

```
sudo umount /mnt/boot
sudo umount /mnt/root
sudo losetup -d $LOOP_DEV
```

`raspios_arm64.img` giờ đã chứa kernel bạn tự build.
Có thể flash ra SD bằng Raspberry Pi Imager hoặc `dd`.

---

# ✅ Hoàn Thành
