# Minimal Scheduler (MLFQ SCX) — Hướng dẫn đầy đủ cho Raspberry Pi 4

Dự án này triển khai một **MLFQ scheduler** trên Linux thông qua **sched_ext (SCX)** và **eBPF struct_ops**. Tài liệu sau hướng dẫn toàn bộ quy trình thiết lập môi trường, clone mã nguồn, build BPF object, tạo `vmlinux.h`, và register scheduler trên Raspberry Pi 4.

---

## 📁 Cấu trúc thư mục dự án

```
minimal_scheduler_pios/
├── README.md
├── LICENSE
├── Makefile
├── include/                # chứa vmlinux.h, common.bpf.h
├── bpf/
│   └── mlfq.bpf.c          # source chính của MLFQ
├── scripts/
│   ├── build_vmlinux.sh    # tạo vmlinux.h từ BTF
│   ├── load_scheduler.sh   # load scheduler bằng bpftool
│   └── test_workload.sh    # script chạy workload
└── examples/
    └── cpu_bound.c         # chương trình test CPU-bound
```

---

# 1. Clone dự án

```bash
cd ~
mkdir -p ~/projects && cd ~/projects
git clone https://github.com/ngotienloc/minimal_scheduler_pios.git
cd minimal_scheduler_pios
ls -la
```

Bạn sẽ thấy:

* `bpf/`
* `scripts/`
* `examples/`
* `Makefile`

---

# 2. Clone repo sched-ext (lấy common.bpf.h)

SCX thay đổi cấu trúc thư mục theo phiên bản — bạn phải clone repo chính thức.

```bash
cd ~/projects
git clone https://github.com/sched-ext/scx.git

# tìm file
find scx -type f -name "common.bpf.h" -o -name "user.bpf.h"
```

Ví dụ file nằm tại:

```
scx/scheds/include/scx/common.bpf.h
```

### 📌 Copy header vào project

```bash
cd ~/projects/minimal_scheduler_pios
mkdir -p include
cp ../scx/scheds/include/scx/common.bpf.h include/
cp ../scx/scheds/include/scx/user.bpf.h include/ 2>/dev/null || true
ls include
```

---

# 3. Cài công cụ build trên Raspberry Pi

```bash
sudo apt update
sudo apt install -y clang llvm libbpf-dev libelf-dev build-essential \
    linux-tools-common linux-tools-$(uname -r) bpftool pahole git

clang --version
bpftool version
```

Nếu `bpftool` không tồn tại → xem phần *Troubleshooting*.

---

# 4. Tạo `vmlinux.h`

File này phải được sinh ra từ kernel **đang chạy** trên Raspberry Pi.

```bash
cd ~/projects/minimal_scheduler_pios
chmod +x scripts/build_vmlinux.sh
./scripts/build_vmlinux.sh include/vmlinux.h
```

Kiểm tra:

```bash
ls -l include/vmlinux.h
head -n 20 include/vmlinux.h
```

Nếu lỗi: `/sys/kernel/btf/vmlinux not found` → kernel không bật BTF.

---

# 5. Kiểm tra include trong BPF code

Trong `bpf/mlfq.bpf.c` phải có:

```c
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "common.bpf.h"
```

Makefile phải chứa:

```
-Iinclude
```

---

# 6. Build BPF object

```bash
cd ~/projects/minimal_scheduler_pios
make
```

Kết quả sẽ có:

```
build/mlfq.bpf.o
```

Nếu lỗi compiler → kiểm tra lại include path hoặc vmlinux.h.

---

# 7. Load MLFQ scheduler

```bash
chmod +x scripts/load_scheduler.sh
./scripts/load_scheduler.sh
```

Hoặc thủ công:

```bash
sudo mkdir -p /sys/fs/bpf/sched_ext
sudo bpftool struct_ops register build/mlfq.bpf.o /sys/fs/bpf/sched_ext/mlfq_4q
```

### Kiểm tra:

```bash
sudo bpftool struct_ops show
sudo dmesg | tail -n 50
```

Nếu bị verifier từ chối → xem phần lỗi.

---

# 8. Test workload

Build file test:

```bash
gcc -O2 examples/cpu_bound.c -o examples/cpu_bound
```

Chạy nhiều process CPU-bound:

```bash
./examples/cpu_bound &
./examples/cpu_bound &
./examples/cpu_bound &
```

Theo dõi trace:

```bash
sudo cat /sys/kernel/debug/tracing/trace_pipe
```

Dump nội dung BPF map:

```bash
sudo bpftool map show
sudo bpftool map dump id <MAP_ID>
```

---

# 9. Unload scheduler

```bash
sudo bpftool struct_ops unregister /sys/fs/bpf/sched_ext/mlfq_4q || true
sudo rm -f /sys/fs/bpf/sched_ext/mlfq_4q || true
```

Hoặc:

```bash
./scripts/unload_scheduler.sh
```

---

# 10. Troubleshooting

## ❌ A. Không có `/sys/kernel/btf/vmlinux`

Kernel không hỗ trợ BTF.

Kiểm tra:

```bash
[ -f /proc/config.gz ] && zgrep DEBUG_INFO_BTF /proc/config.gz || true
```

Nếu không có → phải:

* cài kernel có BTF, hoặc
* build kernel mới với:

```
CONFIG_DEBUG_INFO_BTF=y
CONFIG_SCHED_CLASS_EXT=y
```

## ❌ B. Verifier từ chối khi register

Kiểm tra lỗi:

```bash
sudo dmesg -n 8 | tail -n 80
```

Các lỗi phổ biến:

* vòng lặp không bounded
* dùng struct lớn trên stack
* dùng sai BPF map type

## ❌ C. Không tìm thấy `common.bpf.h`

Repo SCX thay đổi cấu trúc theo phiên bản, hãy tìm:

```bash
find scx -name "common.bpf.h"
```

Copy đúng file.

## ❌ D. Lỗi khi make

Thường do:

* thiếu `-Iinclude`
* thiếu `vmlinux.h`

## ❌ E. Không dump được BPF map theo name

Xem toàn bộ map:

```bash
sudo bpftool map show
```

Dump theo ID:

```bash
sudo bpftool map dump id <ID>
```

---

# Ghi chú & Best Practices

* Regenerate `vmlinux.h` khi thay kernel.   
* Unregister trước khi register scheduler mới.
* Tránh struct lớn trên stack trong BPF.
* Với MLFQ, giữ max_entries đủ lớn (2048–4096).

---

Nếu bạn muốn thêm **phần phân tích code MLFQ**, **flow hoạt động**, hoặc **hoàn chỉnh user-space monitor**, mình có thể viết tiếp.
