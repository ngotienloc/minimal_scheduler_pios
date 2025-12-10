# Custom Raspberry Pi Kernel Build (SCX & eBPF Ready)

Dự án này cung cấp quy trình chuẩn để cross-compile **Linux Kernel 6.12.y** cho Raspberry Pi 4.
Kernel này được tối ưu hóa và cấu hình đặc biệt để hỗ trợ:

* **sched_ext (SCX):** Cơ chế lập lịch mở rộng (Extensible Scheduling Class) cho phép chạy scheduler tùy chỉnh từ userspace (ví dụ: scx_rusty, scx_simple).
* **eBPF (Extended BPF):** Hỗ trợ đầy đủ CO-RE (Compile Once – Run Everywhere) và BTF (BPF Type Format).

## 📂 1. Cấu Trúc Thư Mục Dự Án

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
