# 🧱 Custom Raspberry Pi Kernel Build (SCX & eBPF Ready)

Build **Linux Kernel 6.12.y** tùy chỉnh cho **Raspberry Pi 4**, hỗ trợ đầy đủ:

* **sched_ext (SCX)** — chạy scheduler tùy chỉnh từ userspace.
* **eBPF CO-RE + BTF** — biên dịch một lần, chạy mọi nơi.

Mọi bước đã được sắp xếp lại rõ ràng, gọn gàng và dễ theo dõi.

---

## 📂 1. Cấu Trúc Thư Mục

```
pios/
├── kernel/
│   └── linux/            # Kernel source (rpi-6.12.y)
│
├── build_artifacts/      # Kết quả build
│   ├── .config
│   ├── Image
│   ├── System.map
│   ├── vmlinux
│   └── modules/
│
├── device-tree/          # DTB + Overlays
│   ├── *.dtb
│   └── overlays/
│       └── *.dtbo
│
└── images/
    └── raspios_arm64.img
```

---

## 🛠️ 2. Chuẩn Bị Môi Trường (Host Build Machine)

### 2.1. Cài đặt phụ thuộc

```bash
sudo apt update
sudo apt install -y build-essential bc bison flex libssl-dev libncurses-dev \
libelf-dev libelf1 dwarves device-tree-compiler git rsync python3 python3-pip \
gcc-aarch64-linux-gnu
```

> **dwarves** (pahole) bắt buộc để tạo BTF.

### 2.2. Tạo thư mục & tải source

```bash
mkdir -p ~/pios/{kernel,build_artifacts,device-tree/overlays,images}

cd ~/pios/kernel/
git clone --depth=1 --branch rpi-6.12.y https://github.com/raspberrypi/linux.git linux

cd ~/pios/images/
wget https://downloads.raspberrypi.com/raspios_lite_arm64_latest -O raspios_arm64.img.xz
unxz raspios_arm64.img.xz
```

---

## ⚙️ 3. Cấu Hình Kernel

### 3.1. Load defconfig chuẩn

```bash
cd ~/pios/kernel/linux/
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- bcm2711_defconfig
```

### 3.2. Mở Menuconfig

```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- menuconfig
```

**Các mục cần bật:**

* *General setup → Local version*: `-scx-custom-mlfq`
* *Kernel hacking → Debug information*: chọn **DWARF v4**
* *Generate BTF type information*: bật `[*]`
* *General setup → BPF subsystem*:

  * Enable BPF JIT compiler
  * Enable BPF syscall
* *Scheduler features*:

  * `[*] Extensible Scheduling Class`

---

## 🏗️ 4. Build Kernel

```bash
make -j$(nproc) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- Image modules dtbs
```

### Copy artifacts

```bash
cp .config ../../build_artifacts/
cp arch/arm64/boot/Image ../../build_artifacts/
cp arch/arm64/boot/dts/broadcom/*.dtb ../../device-tree/
cp arch/arm64/boot/dts/broadcom/overlays/*.dtbo ../../device-tree/overlays/
cp arch/arm64/boot/dts/broadcom/overlays/README ../../device-tree/overlays/
```

### Install modules

```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- INSTALL_MOD_PATH=../../build_artifacts modules_install
```

---

## 📦 5. Đóng Gói Kernel Vào OS Image

```bash
cd ~/pios/images
IMG=raspios_arm64.img

LOOP_DEV=$(sudo losetup --show -fP $IMG)
echo "Mounted: $LOOP_DEV"

sudo mkdir -p /mnt/boot /mnt/root
sudo mount ${LOOP_DEV}p1 /mnt/boot
sudo mount ${LOOP_DEV}p2 /mnt/root

sudo cp ~/pios/build_artifacts/Image /mnt/boot/kernel8.img
sudo cp ~/pios/device-tree/*.dtb /mnt/boot/
sudo cp ~/pios/device-tree/overlays/*.dtbo /mnt/boot/overlays/
sudo cp ~/pios/device-tree/overlays/README /mnt/boot/overlays/

sudo cp -r ~/pios/build_artifacts/lib/modules/* /mnt/root/lib/modules/

grep -qxF 'arm_64bit=1' /mnt/boot/config.txt || echo 'arm_64bit=1' | sudo tee -a /mnt/boot/config.txt

sudo umount /mnt/boot
sudo umount /mnt/root
sudo losetup -d $LOOP_DEV
```

---

## ✅ 6. Kiểm Tra Sau Khi Boot

### Kiểm tra phiên bản kernel

```bash
uname -r
```

Kỳ vọng: `6.12.y-scx-custom-mlfq`

### Kiểm tra SCX

```bash
ls /sys/kernel/sched_ext/
```

Nếu thấy thư mục → SCX đã hoạt động.

### Kiểm tra BTF

```bash
ls -lh /sys/kernel/btf/vmlinux
```

Nếu file tồn tại (~4–6 MB) → BTF OK.

---

✨ README đã được làm gọn, rõ ràng và chuyên nghiệp hơn. Bạn muốn mình thêm banner ASCII, bảng tóm tắt lệnh, hay badge GitHub không?
